// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/extensions/extension_window_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/result_codes.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::UserMetricsAction;

// static
void ExtensionFunctionDeleteTraits::Destruct(const ExtensionFunction* x) {
  x->Destruct();
}

UIThreadExtensionFunction::RenderViewHostTracker::RenderViewHostTracker(
    UIThreadExtensionFunction* function,
    RenderViewHost* render_view_host)
    : content::RenderViewHostObserver(render_view_host),
      function_(function) {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 content::Source<RenderViewHost>(function->render_view_host()));
}

void UIThreadExtensionFunction::RenderViewHostTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CHECK(type == content::NOTIFICATION_RENDER_VIEW_HOST_DELETED);
  CHECK(content::Source<RenderViewHost>(source).ptr() ==
        function_->render_view_host());
  function_->SetRenderViewHost(NULL);
}

void UIThreadExtensionFunction::RenderViewHostTracker::RenderViewHostDestroyed(
    RenderViewHost* render_view_host) {
  // Overidding the default behavior of RenderViewHostObserver which is to
  // delete this. In our case, we'll be deleted when the
  // UIThreadExtensionFunction that contains us goes away.
}

bool UIThreadExtensionFunction::RenderViewHostTracker::OnMessageReceived(
    const IPC::Message& message) {
  return function_->OnMessageReceivedFromRenderView(message);
}

ExtensionFunction::ExtensionFunction()
    : request_id_(-1),
      profile_id_(NULL),
      has_callback_(false),
      include_incognito_(false),
      user_gesture_(false),
      args_(NULL),
      bad_message_(false) {
}

ExtensionFunction::~ExtensionFunction() {
}

UIThreadExtensionFunction* ExtensionFunction::AsUIThreadExtensionFunction() {
  return NULL;
}

IOThreadExtensionFunction* ExtensionFunction::AsIOThreadExtensionFunction() {
  return NULL;
}

void ExtensionFunction::OnQuotaExceeded() {
  error_ = QuotaLimitHeuristic::kGenericOverQuotaError;
  SendResponse(false);
}

void ExtensionFunction::SetArgs(const base::ListValue* args) {
  DCHECK(!args_.get());  // Should only be called once.
  args_.reset(args->DeepCopy());
}

const Value* ExtensionFunction::GetResultValue() {
  return result_.get();
}

const std::string ExtensionFunction::GetError() {
  return error_;
}

void ExtensionFunction::SetError(const std::string& error) {
  error_ = error;
}

void ExtensionFunction::Run() {
  if (!RunImpl())
    SendResponse(false);
}

bool ExtensionFunction::ShouldSkipQuotaLimiting() const {
  return false;
}

bool ExtensionFunction::HasOptionalArgument(size_t index) {
  Value* value;
  return args_->Get(index, &value) && !value->IsType(Value::TYPE_NULL);
}

void ExtensionFunction::SendResponseImpl(base::ProcessHandle process,
                                         IPC::Message::Sender* ipc_sender,
                                         int routing_id,
                                         bool success) {
  DCHECK(ipc_sender);
  if (bad_message_) {
    HandleBadMessage(process);
    return;
  }

  // Value objects can't be directly serialized in our IPC code, so we wrap the
  // result_ Value with a ListValue (also transferring ownership of result_).
  base::ListValue result_wrapper;
  if (result_.get())
    result_wrapper.Append(result_.release());

  ipc_sender->Send(new ExtensionMsg_Response(
      routing_id, request_id_, success, result_wrapper, GetError()));
}

void ExtensionFunction::HandleBadMessage(base::ProcessHandle process) {
  LOG(ERROR) << "bad extension message " << name_ << " : terminating renderer.";
  if (content::RenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  } else {
    content::RecordAction(UserMetricsAction("BadMessageTerminate_EFD"));
    if (process)
      base::KillProcess(process, content::RESULT_CODE_KILLED_BAD_MESSAGE,
                        false);
  }
}
UIThreadExtensionFunction::UIThreadExtensionFunction()
    : render_view_host_(NULL), profile_(NULL), delegate_(NULL) {
}

UIThreadExtensionFunction::~UIThreadExtensionFunction() {
  if (dispatcher() && render_view_host())
    dispatcher()->OnExtensionFunctionCompleted(GetExtension());
}

UIThreadExtensionFunction*
UIThreadExtensionFunction::AsUIThreadExtensionFunction() {
  return this;
}

bool UIThreadExtensionFunction::OnMessageReceivedFromRenderView(
    const IPC::Message& message) {
  return false;
}

void UIThreadExtensionFunction::Destruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void UIThreadExtensionFunction::SetRenderViewHost(
    RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;
  tracker_.reset(render_view_host ?
      new RenderViewHostTracker(this, render_view_host) : NULL);
}

// TODO(stevenjb): Replace this with GetExtensionWindowController().
Browser* UIThreadExtensionFunction::GetCurrentBrowser() {
  // If the delegate has an associated browser, return it.
  ExtensionWindowController* window_controller =
      dispatcher()->delegate()->GetExtensionWindowController();
  if (window_controller) {
    Browser* browser = window_controller->GetBrowser();
    if (browser)
      return browser;
  }

  // Otherwise, try to default to a reasonable browser. If |include_incognito_|
  // is true, we will also search browsers in the incognito version of this
  // profile. Note that the profile may already be incognito, in which case
  // we will search the incognito version only, regardless of the value of
  // |include_incognito|.
  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->GetProcess()->GetBrowserContext());
  Browser* browser = BrowserList::FindAnyBrowser(profile, include_incognito_);

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return browser;
}

ExtensionWindowController*
UIThreadExtensionFunction::GetExtensionWindowController() {
  // If the delegate has an associated window controller, return it.
  ExtensionWindowController* window_controller =
      dispatcher()->delegate()->GetExtensionWindowController();
  if (window_controller)
    return window_controller;

  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->GetProcess()->GetBrowserContext());
  ExtensionWindowList::ProfileMatchType match_type = include_incognito_
      ? ExtensionWindowController::MATCH_INCOGNITO
      : ExtensionWindowController::MATCH_NORMAL_ONLY;
  return ExtensionWindowList::GetInstance()->CurrentWindow(profile, match_type);
}

void UIThreadExtensionFunction::SendResponse(bool success) {
  if (delegate_) {
    delegate_->OnSendResponse(this, success, bad_message_);
  } else {
    if (!render_view_host_ || !dispatcher())
      return;

    SendResponseImpl(render_view_host_->GetProcess()->GetHandle(),
                     render_view_host_,
                     render_view_host_->GetRoutingID(),
                     success);
  }
}

IOThreadExtensionFunction::IOThreadExtensionFunction()
    : routing_id_(-1) {
}

IOThreadExtensionFunction::~IOThreadExtensionFunction() {
}

IOThreadExtensionFunction*
IOThreadExtensionFunction::AsIOThreadExtensionFunction() {
  return this;
}

void IOThreadExtensionFunction::Destruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void IOThreadExtensionFunction::SendResponse(bool success) {
  if (!ipc_sender())
    return;

  SendResponseImpl(ipc_sender()->peer_handle(),
                   ipc_sender(), routing_id_, success);
}

AsyncExtensionFunction::AsyncExtensionFunction() {
}

AsyncExtensionFunction::~AsyncExtensionFunction() {
}

SyncExtensionFunction::SyncExtensionFunction() {
}

SyncExtensionFunction::~SyncExtensionFunction() {
}

void SyncExtensionFunction::Run() {
  SendResponse(RunImpl());
}

SyncIOThreadExtensionFunction::SyncIOThreadExtensionFunction() {
}

SyncIOThreadExtensionFunction::~SyncIOThreadExtensionFunction() {
}

void SyncIOThreadExtensionFunction::Run() {
  SendResponse(RunImpl());
}
