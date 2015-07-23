// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_frame_helper.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/content_watcher.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/messaging_bindings.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

namespace {

base::LazyInstance<std::set<const ExtensionFrameHelper*>> g_frame_helpers =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if the render frame corresponding with |frame_helper| matches
// the given criteria.
bool RenderFrameMatches(const ExtensionFrameHelper* frame_helper,
                        ViewType match_view_type,
                        int match_window_id,
                        const std::string& match_extension_id) {
  if (match_view_type != VIEW_TYPE_INVALID &&
      frame_helper->view_type() != match_view_type)
    return false;
  GURL url = frame_helper->render_frame()->GetWebFrame()->document().url();
  if (!url.SchemeIs(kExtensionScheme))
    return false;
  if (url.host() != match_extension_id)
    return false;
  if (match_window_id != extension_misc::kUnknownWindowId &&
      frame_helper->browser_window_id() != match_window_id)
    return false;
  return true;
}

}  // namespace

ExtensionFrameHelper::ExtensionFrameHelper(content::RenderFrame* render_frame,
                                           Dispatcher* extension_dispatcher)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ExtensionFrameHelper>(render_frame),
      view_type_(VIEW_TYPE_INVALID),
      tab_id_(-1),
      browser_window_id_(-1),
      extension_dispatcher_(extension_dispatcher),
      did_create_current_document_element_(false) {
  g_frame_helpers.Get().insert(this);
}

ExtensionFrameHelper::~ExtensionFrameHelper() {
  g_frame_helpers.Get().erase(this);
}

// static
std::vector<content::RenderFrame*> ExtensionFrameHelper::GetExtensionFrames(
    const std::string& extension_id,
    int browser_window_id,
    ViewType view_type) {
  std::vector<content::RenderFrame*> render_frames;
  for (const ExtensionFrameHelper* helper : g_frame_helpers.Get()) {
    if (RenderFrameMatches(helper, view_type, browser_window_id, extension_id))
      render_frames.push_back(helper->render_frame());
  }
  return render_frames;
}

// static
content::RenderFrame* ExtensionFrameHelper::GetBackgroundPageFrame(
    const std::string& extension_id) {
  for (const ExtensionFrameHelper* helper : g_frame_helpers.Get()) {
    if (RenderFrameMatches(helper, VIEW_TYPE_EXTENSION_BACKGROUND_PAGE,
                           extension_misc::kUnknownWindowId, extension_id)) {
      blink::WebLocalFrame* web_frame = helper->render_frame()->GetWebFrame();
      // Check if this is the top frame.
      if (web_frame->top() == web_frame)
        return helper->render_frame();
    }
  }
  return nullptr;
}

// static
bool ExtensionFrameHelper::IsContextForEventPage(const ScriptContext* context) {
  content::RenderFrame* render_frame = context->GetRenderFrame();
  return context->extension() && render_frame &&
         BackgroundInfo::HasLazyBackgroundPage(context->extension()) &&
         ExtensionFrameHelper::Get(render_frame)->view_type() ==
              VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

void ExtensionFrameHelper::DidCreateDocumentElement() {
  did_create_current_document_element_ = true;
  extension_dispatcher_->DidCreateDocumentElement(
      render_frame()->GetWebFrame());
}

void ExtensionFrameHelper::DidCreateNewDocument() {
  did_create_current_document_element_ = false;
}

void ExtensionFrameHelper::DidMatchCSS(
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  extension_dispatcher_->content_watcher()->DidMatchCSS(
      render_frame()->GetWebFrame(), newly_matching_selectors,
      stopped_matching_selectors);
}

void ExtensionFrameHelper::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int extension_group,
    int world_id) {
  extension_dispatcher_->DidCreateScriptContext(
      render_frame()->GetWebFrame(), context, extension_group, world_id);
}

void ExtensionFrameHelper::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  extension_dispatcher_->WillReleaseScriptContext(
      render_frame()->GetWebFrame(), context, world_id);
}

bool ExtensionFrameHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionFrameHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect,
                        OnExtensionDispatchOnConnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnExtensionDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect,
                        OnExtensionDispatchOnDisconnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetTabId, OnExtensionSetTabId)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateBrowserWindowId,
                        OnUpdateBrowserWindowId)
    IPC_MESSAGE_HANDLER(ExtensionMsg_NotifyRenderViewType,
                        OnNotifyRendererViewType)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Response, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnExtensionMessageInvoke)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionFrameHelper::OnExtensionDispatchOnConnect(
    int target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id) {
  MessagingBindings::DispatchOnConnect(
      extension_dispatcher_->script_context_set(),
      target_port_id,
      channel_name,
      source,
      info,
      tls_channel_id,
      render_frame());
}

void ExtensionFrameHelper::OnExtensionDeliverMessage(int target_id,
                                                     const Message& message) {
  MessagingBindings::DeliverMessage(
      extension_dispatcher_->script_context_set(), target_id, message,
      render_frame());
}

void ExtensionFrameHelper::OnExtensionDispatchOnDisconnect(
    int port_id,
    const std::string& error_message) {
  MessagingBindings::DispatchOnDisconnect(
      extension_dispatcher_->script_context_set(), port_id, error_message,
      render_frame());
}

void ExtensionFrameHelper::OnExtensionSetTabId(int tab_id) {
  CHECK_EQ(tab_id_, -1);
  CHECK_GE(tab_id, 0);
  tab_id_ = tab_id;
}

void ExtensionFrameHelper::OnUpdateBrowserWindowId(int browser_window_id) {
  browser_window_id_ = browser_window_id;
}

void ExtensionFrameHelper::OnNotifyRendererViewType(ViewType type) {
  // TODO(devlin): It'd be really nice to be able to
  // DCHECK_EQ(VIEW_TYPE_INVALID, view_type_) here.
  view_type_ = type;
}

void ExtensionFrameHelper::OnExtensionResponse(int request_id,
                                               bool success,
                                               const base::ListValue& response,
                                               const std::string& error) {
  extension_dispatcher_->OnExtensionResponse(request_id,
                                             success,
                                             response,
                                             error);
}

void ExtensionFrameHelper::OnExtensionMessageInvoke(
    const std::string& extension_id,
    const std::string& module_name,
    const std::string& function_name,
    const base::ListValue& args,
    bool user_gesture) {
  extension_dispatcher_->InvokeModuleSystemMethod(render_frame(), extension_id,
                                                  module_name, function_name,
                                                  args, user_gesture);
}

}  // namespace extensions
