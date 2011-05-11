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

ExtensionFunction::RenderViewHostTracker::RenderViewHostTracker(
    ExtensionFunction* function)
    : function_(function) {
  registrar_.Add(this,
                 NotificationType::RENDER_VIEW_HOST_DELETED,
                 Source<RenderViewHost>(function->render_view_host()));
}

void ExtensionFunction::RenderViewHostTracker::Observe(
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
      profile_(NULL),
      has_callback_(false),
      include_incognito_(false),
      user_gesture_(false) {
}

ExtensionFunction::~ExtensionFunction() {
}

void ExtensionFunction::SetRenderViewHost(RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;
  tracker_.reset(render_view_host ? new RenderViewHostTracker(this) : NULL);
}

const Extension* ExtensionFunction::GetExtension() {
  ExtensionService* service = profile_->GetExtensionService();
  DCHECK(service);
  return service->GetExtensionById(extension_id_, false);
}

Browser* ExtensionFunction::GetCurrentBrowser() {
  return dispatcher()->GetCurrentBrowser(render_view_host_, include_incognito_);
}

AsyncExtensionFunction::AsyncExtensionFunction()
    : args_(NULL), bad_message_(false) {
}

AsyncExtensionFunction::~AsyncExtensionFunction() {
}

void AsyncExtensionFunction::SetArgs(const ListValue* args) {
  DCHECK(!args_.get());  // Should only be called once.
  args_.reset(args->DeepCopy());
}

const std::string AsyncExtensionFunction::GetResult() {
  std::string json;
  // Some functions might not need to return any results.
  if (result_.get())
    base::JSONWriter::Write(result_.get(), false, &json);
  return json;
}

const std::string AsyncExtensionFunction::GetError() {
  return error_;
}

void AsyncExtensionFunction::Run() {
  if (!RunImpl())
    SendResponse(false);
}

void AsyncExtensionFunction::SendResponse(bool success) {
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

void AsyncExtensionFunction::HandleBadMessage() {
  LOG(ERROR) << "bad extension message " << name_ << " : terminating renderer.";
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  } else {
    NOTREACHED();
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_EFD"));
    base::KillProcess(render_view_host_->process()->GetHandle(),
                      ResultCodes::KILLED_BAD_MESSAGE, false);
  }
}

bool AsyncExtensionFunction::HasOptionalArgument(size_t index) {
  Value* value;
  return args_->Get(index, &value) && !value->IsType(Value::TYPE_NULL);
}

SyncExtensionFunction::SyncExtensionFunction() {
}

SyncExtensionFunction::~SyncExtensionFunction() {
}

void SyncExtensionFunction::Run() {
  SendResponse(RunImpl());
}
