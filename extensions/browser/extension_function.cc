// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function.h"

#include "base/logging.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using extensions::ErrorUtils;
using extensions::ExtensionAPI;
using extensions::Feature;

namespace {

class ArgumentListResponseValue
    : public ExtensionFunction::ResponseValueObject {
 public:
  ArgumentListResponseValue(const std::string& function_name,
                            const char* title,
                            ExtensionFunction* function,
                            scoped_ptr<base::ListValue> result)
      : function_name_(function_name), title_(title) {
    if (function->GetResultList()) {
      DCHECK_EQ(function->GetResultList(), result.get())
          << "The result set on this function (" << function_name_ << ") "
          << "either by calling SetResult() or directly modifying |result_| is "
          << "different to the one passed to " << title_ << "(). "
          << "The best way to fix this problem is to exclusively use " << title_
          << "(). SetResult() and |result_| are deprecated.";
    } else {
      function->SetResultList(result.Pass());
    }
    // It would be nice to DCHECK(error.empty()) but some legacy extension
    // function implementations... I'm looking at chrome.input.ime... do this
    // for some reason.
  }

  virtual ~ArgumentListResponseValue() {}

  virtual bool Apply() OVERRIDE { return true; }

 private:
  std::string function_name_;
  const char* title_;
};

class ErrorResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  ErrorResponseValue(ExtensionFunction* function, const std::string& error) {
    // It would be nice to DCHECK(!error.empty()) but too many legacy extension
    // function implementations don't set error but signal failure.
    function->SetError(error);
  }

  virtual ~ErrorResponseValue() {}

  virtual bool Apply() OVERRIDE { return false; }
};

class BadMessageResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  explicit BadMessageResponseValue(ExtensionFunction* function) {
    function->set_bad_message(true);
    NOTREACHED() << function->name() << ": bad message";
  }

  virtual ~BadMessageResponseValue() {}

  virtual bool Apply() OVERRIDE { return false; }
};

class RespondNowAction : public ExtensionFunction::ResponseActionObject {
 public:
  typedef base::Callback<void(bool)> SendResponseCallback;
  RespondNowAction(ExtensionFunction::ResponseValue result,
                   const SendResponseCallback& send_response)
      : result_(result.Pass()), send_response_(send_response) {}
  virtual ~RespondNowAction() {}

  virtual void Execute() OVERRIDE { send_response_.Run(result_->Apply()); }

 private:
  ExtensionFunction::ResponseValue result_;
  SendResponseCallback send_response_;
};

class RespondLaterAction : public ExtensionFunction::ResponseActionObject {
 public:
  virtual ~RespondLaterAction() {}

  virtual void Execute() OVERRIDE {}
};

}  // namespace

// static
void ExtensionFunctionDeleteTraits::Destruct(const ExtensionFunction* x) {
  x->Destruct();
}

// Helper class to track the lifetime of ExtensionFunction's RenderViewHost or
// RenderFrameHost  pointer and NULL it out when it dies. It also allows us to
// filter IPC messages coming from the RenderViewHost/RenderFrameHost.
class UIThreadExtensionFunction::RenderHostTracker
    : public content::WebContentsObserver {
 public:
  explicit RenderHostTracker(UIThreadExtensionFunction* function)
      : content::WebContentsObserver(
            function->render_view_host() ?
                WebContents::FromRenderViewHost(function->render_view_host()) :
                WebContents::FromRenderFrameHost(
                    function->render_frame_host())),
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
  virtual void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) OVERRIDE {
    if (render_frame_host != function_->render_frame_host())
      return;

    function_->SetRenderFrameHost(NULL);
  }

  virtual bool OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) OVERRIDE {
    DCHECK(render_frame_host);
    if (render_frame_host == function_->render_frame_host())
      return function_->OnMessageReceived(message);
    else
      return false;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return function_->OnMessageReceived(message);
  }

  UIThreadExtensionFunction* function_;

  DISALLOW_COPY_AND_ASSIGN(RenderHostTracker);
};

ExtensionFunction::ExtensionFunction()
    : request_id_(-1),
      profile_id_(NULL),
      has_callback_(false),
      include_incognito_(false),
      user_gesture_(false),
      bad_message_(false),
      histogram_value_(extensions::functions::UNKNOWN),
      source_tab_id_(-1) {
}

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

void ExtensionFunction::SetResultList(scoped_ptr<base::ListValue> results) {
  results_ = results.Pass();
}

const base::ListValue* ExtensionFunction::GetResultList() const {
  return results_.get();
}

std::string ExtensionFunction::GetError() const {
  return error_;
}

void ExtensionFunction::SetError(const std::string& error) {
  error_ = error;
}

ExtensionFunction::ResponseValue ExtensionFunction::NoArguments() {
  return ResponseValue(new ArgumentListResponseValue(
      name(), "NoArguments", this, make_scoped_ptr(new base::ListValue())));
}

ExtensionFunction::ResponseValue ExtensionFunction::OneArgument(
    base::Value* arg) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(arg);
  return ResponseValue(
      new ArgumentListResponseValue(name(), "OneArgument", this, args.Pass()));
}

ExtensionFunction::ResponseValue ExtensionFunction::TwoArguments(
    base::Value* arg1,
    base::Value* arg2) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(arg1);
  args->Append(arg2);
  return ResponseValue(
      new ArgumentListResponseValue(name(), "TwoArguments", this, args.Pass()));
}

ExtensionFunction::ResponseValue ExtensionFunction::ArgumentList(
    scoped_ptr<base::ListValue> args) {
  return ResponseValue(
      new ArgumentListResponseValue(name(), "ArgumentList", this, args.Pass()));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& error) {
  return ResponseValue(new ErrorResponseValue(this, error));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1) {
  return ResponseValue(
      new ErrorResponseValue(this, ErrorUtils::FormatErrorMessage(format, s1)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1,
    const std::string& s2) {
  return ResponseValue(new ErrorResponseValue(
      this, ErrorUtils::FormatErrorMessage(format, s1, s2)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1,
    const std::string& s2,
    const std::string& s3) {
  return ResponseValue(new ErrorResponseValue(
      this, ErrorUtils::FormatErrorMessage(format, s1, s2, s3)));
}

ExtensionFunction::ResponseValue ExtensionFunction::BadMessage() {
  return ResponseValue(new BadMessageResponseValue(this));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondNow(
    ResponseValue result) {
  return ResponseAction(new RespondNowAction(
      result.Pass(), base::Bind(&ExtensionFunction::SendResponse, this)));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondLater() {
  return ResponseAction(new RespondLaterAction());
}

// static
ExtensionFunction::ResponseAction ExtensionFunction::ValidationFailure(
    ExtensionFunction* function) {
  return function->RespondNow(function->BadMessage());
}

void ExtensionFunction::Respond(ResponseValue result) {
  SendResponse(result->Apply());
}

bool ExtensionFunction::ShouldSkipQuotaLimiting() const {
  return false;
}

bool ExtensionFunction::HasOptionalArgument(size_t index) {
  base::Value* value;
  return args_->Get(index, &value) && !value->IsType(base::Value::TYPE_NULL);
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

void ExtensionFunction::OnRespondingLater(ResponseValue value) {
  SendResponse(value->Apply());
}

UIThreadExtensionFunction::UIThreadExtensionFunction()
    : render_view_host_(NULL),
      render_frame_host_(NULL),
      context_(NULL),
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

bool UIThreadExtensionFunction::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void UIThreadExtensionFunction::Destruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void UIThreadExtensionFunction::SetRenderViewHost(
    RenderViewHost* render_view_host) {
  DCHECK(!render_frame_host_);
  render_view_host_ = render_view_host;
  tracker_.reset(render_view_host ? new RenderHostTracker(this) : NULL);
}

void UIThreadExtensionFunction::SetRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(!render_view_host_);
  render_frame_host_ = render_frame_host;
  tracker_.reset(render_frame_host ? new RenderHostTracker(this) : NULL);
}

content::WebContents* UIThreadExtensionFunction::GetAssociatedWebContents() {
  content::WebContents* web_contents = NULL;
  if (dispatcher())
    web_contents = dispatcher()->delegate()->GetAssociatedWebContents();

  return web_contents;
}

void UIThreadExtensionFunction::SendResponse(bool success) {
  if (delegate_)
    delegate_->OnSendResponse(this, success, bad_message_);
  else
    SendResponseImpl(success);

  if (!transferred_blob_uuids_.empty()) {
    DCHECK(!delegate_) << "Blob transfer not supported with test delegate.";
    GetIPCSender()->Send(
        new ExtensionMsg_TransferBlobs(transferred_blob_uuids_));
  }
}

void UIThreadExtensionFunction::SetTransferredBlobUUIDs(
    const std::vector<std::string>& blob_uuids) {
  DCHECK(transferred_blob_uuids_.empty());  // Should only be called once.
  transferred_blob_uuids_ = blob_uuids;
}

void UIThreadExtensionFunction::WriteToConsole(
    content::ConsoleMessageLevel level,
    const std::string& message) {
  GetIPCSender()->Send(
      new ExtensionMsg_AddMessageToConsole(GetRoutingID(), level, message));
}

IPC::Sender* UIThreadExtensionFunction::GetIPCSender() {
  if (render_view_host_)
    return render_view_host_;
  else
    return render_frame_host_;
}

int UIThreadExtensionFunction::GetRoutingID() {
  if (render_view_host_)
    return render_view_host_->GetRoutingID();
  else
    return render_frame_host_->GetRoutingID();
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

ExtensionFunction::ResponseAction AsyncExtensionFunction::Run() {
  return RunAsync() ? RespondLater() : RespondNow(Error(error_));
}

// static
bool AsyncExtensionFunction::ValidationFailure(
    AsyncExtensionFunction* function) {
  return false;
}

SyncExtensionFunction::SyncExtensionFunction() {
}

SyncExtensionFunction::~SyncExtensionFunction() {
}

ExtensionFunction::ResponseAction SyncExtensionFunction::Run() {
  return RespondNow(RunSync() ? ArgumentList(results_.Pass()) : Error(error_));
}

// static
bool SyncExtensionFunction::ValidationFailure(SyncExtensionFunction* function) {
  return false;
}

SyncIOThreadExtensionFunction::SyncIOThreadExtensionFunction() {
}

SyncIOThreadExtensionFunction::~SyncIOThreadExtensionFunction() {
}

ExtensionFunction::ResponseAction SyncIOThreadExtensionFunction::Run() {
  return RespondNow(RunSync() ? ArgumentList(results_.Pass()) : Error(error_));
}

// static
bool SyncIOThreadExtensionFunction::ValidationFailure(
    SyncIOThreadExtensionFunction* function) {
  return false;
}
