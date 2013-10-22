// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using extensions::ExtensionAPI;
using extensions::Feature;

// static
void ExtensionFunctionDeleteTraits::Destruct(const ExtensionFunction* x) {
  x->Destruct();
}

// Helper class to track the lifetime of ExtensionFunction's RenderViewHost
// pointer and NULL it out when it dies. It also allows us to filter IPC
// messages coming from the RenderViewHost.
class UIThreadExtensionFunction::RenderViewHostTracker
    : public content::WebContentsObserver {
 public:
  explicit RenderViewHostTracker(UIThreadExtensionFunction* function)
      : content::WebContentsObserver(
            WebContents::FromRenderViewHost(function->render_view_host())),
        function_(function) {
  }

 private:
  // content::WebContentsObserver:
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE {
    if (render_view_host != function_->render_view_host())
      return;

    function_->SetRenderViewHost(NULL);
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return function_->OnMessageReceivedFromRenderView(message);
  }

  UIThreadExtensionFunction* function_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTracker);
};

ExtensionFunction::ExtensionFunction()
    : request_id_(-1),
      profile_id_(NULL),
      has_callback_(false),
      include_incognito_(false),
      user_gesture_(false),
      bad_message_(false),
      histogram_value_(extensions::functions::UNKNOWN) {}

ExtensionFunction::~ExtensionFunction() {
}

UIThreadExtensionFunction* ExtensionFunction::AsUIThreadExtensionFunction() {
  return NULL;
}

IOThreadExtensionFunction* ExtensionFunction::AsIOThreadExtensionFunction() {
  return NULL;
}

bool ExtensionFunction::HasPermission() {
  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          name_, extension_, Feature::BLESSED_EXTENSION_CONTEXT, source_url());
  return availability.is_available();
}

void ExtensionFunction::OnQuotaExceeded(const std::string& violation_error) {
  error_ = violation_error;
  SendResponse(false);
}

void ExtensionFunction::SetArgs(const base::ListValue* args) {
  DCHECK(!args_.get());  // Should only be called once.
  args_.reset(args->DeepCopy());
}

void ExtensionFunction::SetResult(base::Value* result) {
  results_.reset(new base::ListValue());
  results_->Append(result);
}

const base::ListValue* ExtensionFunction::GetResultList() {
  return results_.get();
}

const std::string ExtensionFunction::GetError() {
  return error_;
}

void ExtensionFunction::SetError(const std::string& error) {
  error_ = error;
}

void ExtensionFunction::Run() {
  UMA_HISTOGRAM_ENUMERATION("Extensions.FunctionCalls", histogram_value(),
                            extensions::functions::ENUM_BOUNDARY);

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

void ExtensionFunction::SendResponseImpl(bool success) {
  DCHECK(!response_callback_.is_null());

  ResponseType type = success ? SUCCEEDED : FAILED;
  if (bad_message_) {
    type = BAD_MESSAGE;
    LOG(ERROR) << "Bad extension message " << name_;
  }

  // If results were never set, we send an empty argument list.
  if (!results_)
    results_.reset(new base::ListValue());

  response_callback_.Run(type, *results_, GetError());
}

UIThreadExtensionFunction::UIThreadExtensionFunction()
    : render_view_host_(NULL),
      profile_(NULL),
      delegate_(NULL) {
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
  tracker_.reset(render_view_host ? new RenderViewHostTracker(this) : NULL);
}

// TODO(stevenjb): Replace this with GetExtensionWindowController().
Browser* UIThreadExtensionFunction::GetCurrentBrowser() {
  // If the delegate has an associated browser, return it.
  if (dispatcher()) {
    extensions::WindowController* window_controller =
        dispatcher()->delegate()->GetExtensionWindowController();
    if (window_controller) {
      Browser* browser = window_controller->GetBrowser();
      if (browser)
        return browser;
    }
  }

  // Otherwise, try to default to a reasonable browser. If |include_incognito_|
  // is true, we will also search browsers in the incognito version of this
  // profile. Note that the profile may already be incognito, in which case
  // we will search the incognito version only, regardless of the value of
  // |include_incognito|. Look only for browsers on the active desktop as it is
  // preferable to pretend no browser is open then to return a browser on
  // another desktop.
  if (render_view_host_) {
    Profile* profile = Profile::FromBrowserContext(
        render_view_host_->GetProcess()->GetBrowserContext());
    Browser* browser = chrome::FindAnyBrowser(profile, include_incognito_,
                                              chrome::GetActiveDesktop());
    if (browser)
      return browser;
  }

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here, or
  // all of this profile's browser windows may have been closed.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return NULL;
}

content::WebContents* UIThreadExtensionFunction::GetAssociatedWebContents() {
  if (dispatcher()) {
    content::WebContents* web_contents =
        dispatcher()->delegate()->GetAssociatedWebContents();
    if (web_contents)
      return web_contents;
  }

  Browser* browser = GetCurrentBrowser();
  if (!browser)
    return NULL;
  return browser->tab_strip_model()->GetActiveWebContents();
}

extensions::WindowController*
UIThreadExtensionFunction::GetExtensionWindowController() {
  // If the delegate has an associated window controller, return it.
  if (dispatcher()) {
    extensions::WindowController* window_controller =
        dispatcher()->delegate()->GetExtensionWindowController();
    if (window_controller)
      return window_controller;
  }

  return extensions::WindowControllerList::GetInstance()->
      CurrentWindowForFunction(this);
}

bool UIThreadExtensionFunction::CanOperateOnWindow(
    const extensions::WindowController* window_controller) const {
  const extensions::Extension* extension = GetExtension();
  // |extension| is NULL for unit tests only.
  if (extension != NULL && !window_controller->IsVisibleToExtension(extension))
    return false;

  if (profile() == window_controller->profile())
    return true;

  if (!include_incognito())
    return false;

  return profile()->HasOffTheRecordProfile() &&
      profile()->GetOffTheRecordProfile() == window_controller->profile();
}

void UIThreadExtensionFunction::SendResponse(bool success) {
  if (delegate_)
    delegate_->OnSendResponse(this, success, bad_message_);
  else
    SendResponseImpl(success);
}

void UIThreadExtensionFunction::WriteToConsole(
    content::ConsoleMessageLevel level,
    const std::string& message) {
  render_view_host_->Send(new ExtensionMsg_AddMessageToConsole(
      render_view_host_->GetRoutingID(), level, message));
}

IOThreadExtensionFunction::IOThreadExtensionFunction()
    : routing_id_(MSG_ROUTING_NONE) {
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
  SendResponseImpl(success);
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
