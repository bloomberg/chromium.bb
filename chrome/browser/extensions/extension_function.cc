// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "content/common/result_codes.h"

// static
void ExtensionFunctionDeleteTraits::Destruct(const ExtensionFunction* x) {
  x->Destruct();
}

UIThreadExtensionFunction::RenderViewHostTracker::RenderViewHostTracker(
    UIThreadExtensionFunction* function)
    : function_(function) {
  registrar_.Add(this,
                 NotificationType::RENDER_VIEW_HOST_DELETED,
                 Source<RenderViewHost>(function->render_view_host()));
}

void UIThreadExtensionFunction::RenderViewHostTracker::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  CHECK(type == NotificationType::RENDER_VIEW_HOST_DELETED);
  CHECK(Source<RenderViewHost>(source).ptr() ==
        function_->render_view_host());
  function_->SetRenderViewHost(NULL);
}

ExtensionFunction::ExtensionFunction()
    : request_id_(-1),
      profile_id_(0),
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

void ExtensionFunction::SetArgs(const ListValue* args) {
  DCHECK(!args_.get());  // Should only be called once.
  args_.reset(args->DeepCopy());
}

const std::string ExtensionFunction::GetResult() {
  std::string json;
  // Some functions might not need to return any results.
  if (result_.get())
    base::JSONWriter::Write(result_.get(), false, &json);
  return json;
}

const std::string ExtensionFunction::GetError() {
  return error_;
}

void ExtensionFunction::Run() {
  if (!RunImpl())
    SendResponse(false);
}

bool ExtensionFunction::HasOptionalArgument(size_t index) {
  Value* value;
  return args_->Get(index, &value) && !value->IsType(Value::TYPE_NULL);
}

UIThreadExtensionFunction::UIThreadExtensionFunction()
    : profile_(NULL) {
}

UIThreadExtensionFunction::~UIThreadExtensionFunction() {
}

UIThreadExtensionFunction*
UIThreadExtensionFunction::AsUIThreadExtensionFunction() {
  return this;
}

void UIThreadExtensionFunction::Destruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void UIThreadExtensionFunction::SetRenderViewHost(
    RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;
  tracker_.reset(render_view_host ? new RenderViewHostTracker(this) : NULL);
}

Browser* UIThreadExtensionFunction::GetCurrentBrowser() {
  return dispatcher()->GetCurrentBrowser(render_view_host_, include_incognito_);
}

void UIThreadExtensionFunction::SendResponse(bool success) {
  if (!render_view_host_ || !dispatcher())
    return;
  if (bad_message_) {
    HandleBadMessage();
    return;
  }

  render_view_host_->Send(new ExtensionMsg_Response(
      render_view_host_->routing_id(), request_id_, success,
      GetResult(), GetError()));
}

void UIThreadExtensionFunction::HandleBadMessage() {
  LOG(ERROR) << "bad extension message " << name_ << " : terminating renderer.";
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  } else {
    NOTREACHED();
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_EFD"));
    if (render_view_host_) {
      base::KillProcess(render_view_host_->process()->GetHandle(),
          ResultCodes::KILLED_BAD_MESSAGE, false);
    }
  }
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
