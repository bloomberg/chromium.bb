// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_render_view_helper.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_webstore_bindings.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_bindings_context.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/extension_renderer_context.h"
#include "chrome/renderer/extensions/user_script_idle_scheduler.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/common/json_value_serializer.h"
#include "content/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/resource_fetcher.h"

using WebKit::WebConsoleMessage;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebURLRequest;
using WebKit::WebView;
using webkit_glue::ImageResourceFetcher;
using webkit_glue::ResourceFetcher;

namespace {
// Keeps a mapping from the frame pointer to a UserScriptIdleScheduler object.
// We store this mapping per process, because a frame can jump from one
// document to another with adoptNode, and so having the object be a
// RenderViewObserver means it might miss some notifications after it moves.
typedef std::map<WebFrame*, UserScriptIdleScheduler*> SchedulerMap;
static base::LazyInstance<SchedulerMap> g_schedulers(base::LINKER_INITIALIZED);
}

ExtensionRenderViewHelper::ExtensionRenderViewHelper(
    RenderView* render_view,
    ExtensionRendererContext* extension_renderer_context)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<ExtensionRenderViewHelper>(render_view),
      extension_renderer_context_(extension_renderer_context),
      pending_app_icon_requests_(0),
      view_type_(ViewType::INVALID),
      browser_window_id_(-1) {
}

ExtensionRenderViewHelper::~ExtensionRenderViewHelper() {
}

bool ExtensionRenderViewHelper::InstallWebApplicationUsingDefinitionFile(
    WebFrame* frame, string16* error) {
  // There is an issue of drive-by installs with the below implementation. A web
  // site could force a user to install an app by timing the dialog to come up
  // just before the user clicks.
  //
  // We do show a success UI that allows users to uninstall, but it seems that
  // we might still want to put up an infobar before showing the install dialog.
  //
  // TODO(aa): Figure out this issue before removing the kEnableCrxlessWebApps
  // switch.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrxlessWebApps)) {
    *error = ASCIIToUTF16("CRX-less web apps aren't enabled.");
    return false;
  }

  if (frame != frame->top()) {
    *error = ASCIIToUTF16("Applications can only be installed from the top "
                          "frame.");
    return false;
  }

  if (pending_app_info_.get()) {
    *error = ASCIIToUTF16("An application install is already in progress.");
    return false;
  }

  pending_app_info_.reset(new WebApplicationInfo());
  if (!web_apps::ParseWebAppFromWebDocument(frame, pending_app_info_.get(),
                                            error)) {
    return false;
  }

  if (!pending_app_info_->manifest_url.is_valid()) {
    *error = ASCIIToUTF16("Web application definition not found or invalid.");
    return false;
  }

  app_definition_fetcher_.reset(new ResourceFetcher(
      pending_app_info_->manifest_url, render_view()->webview()->mainFrame(),
      WebURLRequest::TargetIsSubresource,
      NewCallback(
          this,
          &ExtensionRenderViewHelper::DidDownloadApplicationDefinition)));
  return true;
}

void ExtensionRenderViewHelper::InlineWebstoreInstall(
    int install_id, std::string webstore_item_id, GURL requestor_url) {
  Send(new ExtensionHostMsg_InlineWebstoreInstall(
      routing_id(), install_id, webstore_item_id, requestor_url));
}

void ExtensionRenderViewHelper::OnInlineWebstoreInstallResponse(
    int install_id,
    bool success,
    const std::string& error) {
  ChromeWebstoreExtension::HandleInstallResponse(install_id, success, error);
}

bool ExtensionRenderViewHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionRenderViewHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Response, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ExecuteCode, OnExecuteCode)
    IPC_MESSAGE_HANDLER(ExtensionMsg_GetApplicationInfo, OnGetApplicationInfo)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateBrowserWindowId,
                        OnUpdateBrowserWindowId)
    IPC_MESSAGE_HANDLER(ExtensionMsg_NotifyRenderViewType,
                        OnNotifyRendererViewType)
    IPC_MESSAGE_HANDLER(ExtensionMsg_InlineWebstoreInstallResponse,
                        OnInlineWebstoreInstallResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionRenderViewHelper::DidFinishDocumentLoad(WebFrame* frame) {
  extension_renderer_context_->user_script_slave()->InjectScripts(
      frame, UserScript::DOCUMENT_END);

  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i != g_schedulers.Get().end())
    i->second->DidFinishDocumentLoad();
}

void ExtensionRenderViewHelper::DidFinishLoad(WebKit::WebFrame* frame) {
  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i != g_schedulers.Get().end())
    i->second->DidFinishLoad();
}

void ExtensionRenderViewHelper::DidCreateDocumentElement(WebFrame* frame) {
  extension_renderer_context_->user_script_slave()->InjectScripts(
      frame, UserScript::DOCUMENT_START);
}

void ExtensionRenderViewHelper::DidStartProvisionalLoad(
    WebKit::WebFrame* frame) {
  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i != g_schedulers.Get().end())
    i->second->DidStartProvisionalLoad();
}

void ExtensionRenderViewHelper::FrameDetached(WebFrame* frame) {
  // This could be called before DidCreateDataSource, in which case the frame
  // won't be in the map.
  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i == g_schedulers.Get().end())
    return;

  delete i->second;
  g_schedulers.Get().erase(i);
}

void ExtensionRenderViewHelper::DidCreateDataSource(WebFrame* frame,
                                                    WebDataSource* ds) {
  // If there are any app-related fetches in progress, they can be cancelled now
  // since we have navigated away from the page that created them.
  if (!frame->parent()) {
    app_icon_fetchers_.clear();
    app_definition_fetcher_.reset(NULL);
  }

  // Check first if we created a scheduler for the frame, since this function
  // gets called for navigations within the document.
  if (g_schedulers.Get().count(frame))
    return;

  g_schedulers.Get()[frame] = new UserScriptIdleScheduler(
      frame, extension_renderer_context_);
}

void ExtensionRenderViewHelper::OnExtensionResponse(int request_id,
                                          bool success,
                                          const std::string& response,
                                          const std::string& error) {
  ExtensionProcessBindings::HandleResponse(
      request_id, success, response, error);
}

void ExtensionRenderViewHelper::OnExtensionMessageInvoke(
    const std::string& extension_id,
    const std::string& function_name,
    const ListValue& args,
    const GURL& event_url) {
  ExtensionBindingsContext::DispatchChromeHiddenMethod(
      extension_id, function_name, args, render_view(), event_url);
}

void ExtensionRenderViewHelper::OnExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params) {
  WebView* webview = render_view()->webview();
  WebFrame* main_frame = webview->mainFrame();
  if (!main_frame) {
    Send(new ExtensionHostMsg_ExecuteCodeFinished(
        routing_id(), params.request_id, false, ""));
    return;
  }

  // chrome.tabs.executeScript() only supports execution in either the top frame
  // or all frames.  We handle both cases in the top frame.
  SchedulerMap::iterator i = g_schedulers.Get().find(main_frame);
  if (i != g_schedulers.Get().end())
    i->second->ExecuteCode(params);
}

void ExtensionRenderViewHelper::OnGetApplicationInfo(int page_id) {
  WebApplicationInfo app_info;
  if (page_id == render_view()->page_id()) {
    string16 error;
    web_apps::ParseWebAppFromWebDocument(
        render_view()->webview()->mainFrame(), &app_info, &error);
  }

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    if (app_info.icons[i].url.SchemeIs(chrome::kDataScheme)) {
      app_info.icons.erase(app_info.icons.begin() + i);
      --i;
    }
  }

  Send(new ExtensionHostMsg_DidGetApplicationInfo(
      routing_id(), page_id, app_info));
}

void ExtensionRenderViewHelper::OnNotifyRendererViewType(ViewType::Type type) {
  view_type_ = type;
}

void ExtensionRenderViewHelper::OnUpdateBrowserWindowId(int window_id) {
  browser_window_id_ = window_id;
}

void ExtensionRenderViewHelper::DidDownloadApplicationDefinition(
    const WebKit::WebURLResponse& response,
    const std::string& data) {
  scoped_ptr<WebApplicationInfo> app_info(
      pending_app_info_.release());

  JSONStringValueSerializer serializer(data);
  int error_code = 0;
  std::string error_message;
  scoped_ptr<Value> result(serializer.Deserialize(&error_code, &error_message));
  if (!result.get()) {
    AddErrorToRootConsole(UTF8ToUTF16(error_message));
    return;
  }

  string16 error_message_16;
  if (!web_apps::ParseWebAppFromDefinitionFile(result.get(), app_info.get(),
                                               &error_message_16)) {
    AddErrorToRootConsole(error_message_16);
    return;
  }

  if (!app_info->icons.empty()) {
    pending_app_info_.reset(app_info.release());
    pending_app_icon_requests_ =
        static_cast<int>(pending_app_info_->icons.size());
    for (size_t i = 0; i < pending_app_info_->icons.size(); ++i) {
      app_icon_fetchers_.push_back(linked_ptr<ImageResourceFetcher>(
          new ImageResourceFetcher(
              pending_app_info_->icons[i].url,
              render_view()->webview()->mainFrame(),
              static_cast<int>(i),
              pending_app_info_->icons[i].width,
              WebURLRequest::TargetIsFavicon,
              NewCallback(
                  this,
                  &ExtensionRenderViewHelper::DidDownloadApplicationIcon))));
    }
  } else {
    Send(new ExtensionHostMsg_InstallApplication(routing_id(), *app_info));
  }
}

void ExtensionRenderViewHelper::DidDownloadApplicationIcon(
    ImageResourceFetcher* fetcher,
    const SkBitmap& image) {
  pending_app_info_->icons[fetcher->id()].data = image;

  // Remove the image fetcher from our pending list. We're in the callback from
  // ImageResourceFetcher, best to delay deletion.
  RenderView::ImageResourceFetcherList::iterator i;
  for (i = app_icon_fetchers_.begin(); i != app_icon_fetchers_.end(); ++i) {
    if (i->get() == fetcher) {
      i->release();
      app_icon_fetchers_.erase(i);
      break;
    }
  }

  // We're in the callback from the ImageResourceFetcher, best to delay
  // deletion.
  MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);

  if (--pending_app_icon_requests_ > 0)
    return;

  // There is a maximum size of IPC on OS X and Linux that we have run into in
  // some situations. We're not sure what it is, but our hypothesis is in the
  // neighborhood of 1 MB.
  //
  // To be on the safe side, we give ourselves 128 KB for just the image data.
  // This should be more than enough for 128, 48, and 16 px 32-bit icons. If we
  // want to start allowing larger icons (see bug 63406), we'll have to either
  // experiment mor ewith this and find the real limit, or else come up with
  // some alternative way to transmit the icon data to the browser process.
  //
  // See also: bug 63729.
  const size_t kMaxIconSize = 1024 * 128;
  size_t actual_icon_size = 0;
  for (size_t i = 0; i < pending_app_info_->icons.size(); ++i) {
    size_t current_size = pending_app_info_->icons[i].data.getSize();
    if (current_size > kMaxIconSize - actual_icon_size) {
      AddErrorToRootConsole(ASCIIToUTF16(
        "Icons are too large. Maximum total size for app icons is 128 KB."));
      return;
    }
    actual_icon_size += current_size;
  }

  Send(new ExtensionHostMsg_InstallApplication(
      routing_id(), *pending_app_info_));
  pending_app_info_.reset(NULL);
}

void ExtensionRenderViewHelper::AddErrorToRootConsole(const string16& message) {
  if (render_view()->webview() && render_view()->webview()->mainFrame()) {
    render_view()->webview()->mainFrame()->addMessageToConsole(
        WebConsoleMessage(WebConsoleMessage::LevelError, message));
  }
}
