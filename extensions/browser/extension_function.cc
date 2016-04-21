// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
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
                            std::unique_ptr<base::ListValue> result)
      : function_name_(function_name), title_(title) {
    if (function->GetResultList()) {
      DCHECK_EQ(function->GetResultList(), result.get())
          << "The result set on this function (" << function_name_ << ") "
          << "either by calling SetResult() or directly modifying |result_| is "
          << "different to the one passed to " << title_ << "(). "
          << "The best way to fix this problem is to exclusively use " << title_
          << "(). SetResult() and |result_| are deprecated.";
    } else {
      function->SetResultList(std::move(result));
    }
    // It would be nice to DCHECK(error.empty()) but some legacy extension
    // function implementations... I'm looking at chrome.input.ime... do this
    // for some reason.
  }

  ~ArgumentListResponseValue() override {}

  bool Apply() override { return true; }

 private:
  std::string function_name_;
  const char* title_;
};

class ErrorWithArgumentsResponseValue : public ArgumentListResponseValue {
 public:
  ErrorWithArgumentsResponseValue(const std::string& function_name,
                                  const char* title,
                                  ExtensionFunction* function,
                                  std::unique_ptr<base::ListValue> result,
                                  const std::string& error)
      : ArgumentListResponseValue(function_name,
                                  title,
                                  function,
                                  std::move(result)) {
    function->SetError(error);
  }

  ~ErrorWithArgumentsResponseValue() override {}

  bool Apply() override { return false; }
};

class ErrorResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  ErrorResponseValue(ExtensionFunction* function, const std::string& error) {
    // It would be nice to DCHECK(!error.empty()) but too many legacy extension
    // function implementations don't set error but signal failure.
    function->SetError(error);
  }

  ~ErrorResponseValue() override {}

  bool Apply() override { return false; }
};

class BadMessageResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  explicit BadMessageResponseValue(ExtensionFunction* function) {
    function->set_bad_message(true);
    NOTREACHED() << function->name() << ": bad message";
  }

  ~BadMessageResponseValue() override {}

  bool Apply() override { return false; }
};

class RespondNowAction : public ExtensionFunction::ResponseActionObject {
 public:
  typedef base::Callback<void(bool)> SendResponseCallback;
  RespondNowAction(ExtensionFunction::ResponseValue result,
                   const SendResponseCallback& send_response)
      : result_(std::move(result)), send_response_(send_response) {}
  ~RespondNowAction() override {}

  void Execute() override { send_response_.Run(result_->Apply()); }

 private:
  ExtensionFunction::ResponseValue result_;
  SendResponseCallback send_response_;
};

class RespondLaterAction : public ExtensionFunction::ResponseActionObject {
 public:
  ~RespondLaterAction() override {}

  void Execute() override {}
};

// Used in implementation of ScopedUserGestureForTests.
class UserGestureForTests {
 public:
  static UserGestureForTests* GetInstance();

  // Returns true if there is at least one ScopedUserGestureForTests object
  // alive.
  bool HaveGesture();

  // These should be called when a ScopedUserGestureForTests object is
  // created/destroyed respectively.
  void IncrementCount();
  void DecrementCount();

 private:
  UserGestureForTests();
  friend struct base::DefaultSingletonTraits<UserGestureForTests>;

  base::Lock lock_; // for protecting access to count_
  int count_;
};

// static
UserGestureForTests* UserGestureForTests::GetInstance() {
  return base::Singleton<UserGestureForTests>::get();
}

UserGestureForTests::UserGestureForTests() : count_(0) {}

bool UserGestureForTests::HaveGesture() {
  base::AutoLock autolock(lock_);
  return count_ > 0;
}

void UserGestureForTests::IncrementCount() {
  base::AutoLock autolock(lock_);
  ++count_;
}

void UserGestureForTests::DecrementCount() {
  base::AutoLock autolock(lock_);
  --count_;
}


}  // namespace

// static
void ExtensionFunctionDeleteTraits::Destruct(const ExtensionFunction* x) {
  x->Destruct();
}

// Helper class to track the lifetime of ExtensionFunction's RenderFrameHost and
// notify the function when it is deleted, as well as forwarding any messages
// to the ExtensionFunction.
class UIThreadExtensionFunction::RenderFrameHostTracker
    : public content::WebContentsObserver {
 public:
  explicit RenderFrameHostTracker(UIThreadExtensionFunction* function)
      : content::WebContentsObserver(
            WebContents::FromRenderFrameHost(function->render_frame_host())),
        function_(function) {
  }

 private:
  // content::WebContentsObserver:
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host == function_->render_frame_host())
      function_->SetRenderFrameHost(nullptr);
  }

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    return render_frame_host == function_->render_frame_host() &&
        function_->OnMessageReceived(message);
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return function_->OnMessageReceived(message);
  }

  UIThreadExtensionFunction* function_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostTracker);
};

ExtensionFunction::ExtensionFunction()
    : request_id_(-1),
      profile_id_(NULL),
      name_(""),
      has_callback_(false),
      include_incognito_(false),
      user_gesture_(false),
      bad_message_(false),
      histogram_value_(extensions::functions::UNKNOWN),
      source_tab_id_(-1),
      source_context_type_(Feature::UNSPECIFIED_CONTEXT),
      source_process_id_(-1) {
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
          name_, extension_.get(), source_context_type_, source_url());
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

void ExtensionFunction::SetResult(std::unique_ptr<base::Value> result) {
  results_.reset(new base::ListValue());
  results_->Append(std::move(result));
}

void ExtensionFunction::SetResultList(
    std::unique_ptr<base::ListValue> results) {
  results_ = std::move(results);
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

bool ExtensionFunction::user_gesture() const {
  return user_gesture_ || UserGestureForTests::GetInstance()->HaveGesture();
}

ExtensionFunction::ResponseValue ExtensionFunction::NoArguments() {
  return ResponseValue(new ArgumentListResponseValue(
      name(), "NoArguments", this, base::WrapUnique(new base::ListValue())));
}

ExtensionFunction::ResponseValue ExtensionFunction::OneArgument(
    base::Value* arg) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(arg);
  return ResponseValue(new ArgumentListResponseValue(name(), "OneArgument",
                                                     this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::OneArgument(
    std::unique_ptr<base::Value> arg) {
  return OneArgument(arg.release());
}

ExtensionFunction::ResponseValue ExtensionFunction::TwoArguments(
    base::Value* arg1,
    base::Value* arg2) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(arg1);
  args->Append(arg2);
  return ResponseValue(new ArgumentListResponseValue(name(), "TwoArguments",
                                                     this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::ArgumentList(
    std::unique_ptr<base::ListValue> args) {
  return ResponseValue(new ArgumentListResponseValue(name(), "ArgumentList",
                                                     this, std::move(args)));
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

ExtensionFunction::ResponseValue ExtensionFunction::ErrorWithArguments(
    std::unique_ptr<base::ListValue> args,
    const std::string& error) {
  return ResponseValue(new ErrorWithArgumentsResponseValue(
      name(), "ErrorWithArguments", this, std::move(args), error));
}

ExtensionFunction::ResponseValue ExtensionFunction::BadMessage() {
  return ResponseValue(new BadMessageResponseValue(this));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondNow(
    ResponseValue result) {
  return ResponseAction(new RespondNowAction(
      std::move(result), base::Bind(&ExtensionFunction::SendResponse, this)));
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

  response_callback_.Run(type, *results_, GetError(), histogram_value());
}

void ExtensionFunction::OnRespondingLater(ResponseValue value) {
  SendResponse(value->Apply());
}

UIThreadExtensionFunction::UIThreadExtensionFunction()
    : context_(nullptr),
      render_frame_host_(nullptr),
      delegate_(nullptr) {
}

UIThreadExtensionFunction::~UIThreadExtensionFunction() {
  if (dispatcher() && render_frame_host())
    dispatcher()->OnExtensionFunctionCompleted(extension());
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

content::RenderViewHost*
UIThreadExtensionFunction::render_view_host_do_not_use() const {
  return render_frame_host_ ? render_frame_host_->GetRenderViewHost() : nullptr;
}

void UIThreadExtensionFunction::SetRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_NE(render_frame_host_ == nullptr, render_frame_host == nullptr);
  render_frame_host_ = render_frame_host;
  tracker_.reset(
      render_frame_host ? new RenderFrameHostTracker(this) : nullptr);
}

content::WebContents* UIThreadExtensionFunction::GetAssociatedWebContents() {
  content::WebContents* web_contents = NULL;
  if (dispatcher())
    web_contents = dispatcher()->GetAssociatedWebContents();

  return web_contents;
}

content::WebContents* UIThreadExtensionFunction::GetSenderWebContents() {
  return render_frame_host_ ?
      content::WebContents::FromRenderFrameHost(render_frame_host_) : nullptr;
}

void UIThreadExtensionFunction::SendResponse(bool success) {
  if (delegate_)
    delegate_->OnSendResponse(this, success, bad_message_);
  else
    SendResponseImpl(success);

  if (!transferred_blob_uuids_.empty()) {
    DCHECK(!delegate_) << "Blob transfer not supported with test delegate.";
    render_frame_host_->Send(
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
  // Only the main frame handles dev tools messages.
  WebContents::FromRenderFrameHost(render_frame_host_)
      ->GetMainFrame()
      ->AddMessageToConsole(level, message);
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

ExtensionFunction::ScopedUserGestureForTests::ScopedUserGestureForTests() {
  UserGestureForTests::GetInstance()->IncrementCount();
}

ExtensionFunction::ScopedUserGestureForTests::~ScopedUserGestureForTests() {
  UserGestureForTests::GetInstance()->DecrementCount();
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
  return RespondNow(RunSync() ? ArgumentList(std::move(results_))
                              : Error(error_));
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
  return RespondNow(RunSync() ? ArgumentList(std::move(results_))
                              : Error(error_));
}

// static
bool SyncIOThreadExtensionFunction::ValidationFailure(
    SyncIOThreadExtensionFunction* function) {
  return false;
}
