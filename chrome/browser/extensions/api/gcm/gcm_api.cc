// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcm/gcm_api.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/extensions/api/gcm.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

namespace {

const size_t kMaximumMessageSize = 4096;  // in bytes.
const char kCollapseKey[] = "collapse_key";
const char kGoogDotRestrictedPrefix[] = "goog.";
const char kGoogleRestrictedPrefix[] = "google";

// Error messages.
const char kInvalidParameter[] =
    "Function was called with invalid parameters.";
const char kNotSignedIn[] = "Profile was not signed in.";
const char kAsyncOperationPending[] =
    "Asynchronous operation is pending.";
const char kNetworkError[] = "Network error occurred.";
const char kServerError[] = "Server error occurred.";
const char kTtlExceeded[] = "Time-to-live exceeded.";
const char kUnknownError[] = "Unknown error occurred.";

const char* GcmResultToError(gcm::GCMClient::Result result) {
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      return "";
    case gcm::GCMClient::INVALID_PARAMETER:
      return kInvalidParameter;
    case gcm::GCMClient::NOT_SIGNED_IN:
      return kNotSignedIn;
    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
      return kAsyncOperationPending;
    case gcm::GCMClient::NETWORK_ERROR:
      return kNetworkError;
    case gcm::GCMClient::SERVER_ERROR:
      return kServerError;
    case gcm::GCMClient::TTL_EXCEEDED:
      return kTtlExceeded;
    case gcm::GCMClient::UNKNOWN_ERROR:
      return kUnknownError;
    default:
      NOTREACHED() << "Unexpected value of result cannot be converted: "
                   << result;
  }

  // Never reached, but prevents missing return statement warning.
  return "";
}

bool IsMessageKeyValid(const std::string& key) {
  std::string lower = StringToLowerASCII(key);
  return !key.empty() &&
         key.compare(0, arraysize(kCollapseKey) - 1, kCollapseKey) != 0 &&
         lower.compare(0,
                       arraysize(kGoogleRestrictedPrefix) - 1,
                       kGoogleRestrictedPrefix) != 0 &&
         lower.compare(0,
                       arraysize(kGoogDotRestrictedPrefix),
                       kGoogDotRestrictedPrefix) != 0;
}

}  // namespace

namespace extensions {

bool GcmApiFunction::RunImpl() {
  if (!IsGcmApiEnabled())
    return false;

  return DoWork();
}

bool GcmApiFunction::IsGcmApiEnabled() const {
  Profile* profile = Profile::FromBrowserContext(browser_context());

  // GCM is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return false;

  return gcm::GCMProfileService::GetGCMEnabledState(profile) !=
      gcm::GCMProfileService::ALWAYS_DISABLED;
}

gcm::GCMProfileService* GcmApiFunction::GCMProfileService() const {
  return gcm::GCMProfileServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
}

GcmRegisterFunction::GcmRegisterFunction() {}

GcmRegisterFunction::~GcmRegisterFunction() {}

bool GcmRegisterFunction::DoWork() {
  scoped_ptr<api::gcm::Register::Params> params(
      api::gcm::Register::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GCMProfileService()->Register(
      GetExtension()->id(),
      params->sender_ids,
      base::Bind(&GcmRegisterFunction::CompleteFunctionWithResult, this));

  return true;
}

void GcmRegisterFunction::CompleteFunctionWithResult(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  SetResult(base::Value::CreateStringValue(registration_id));
  SetError(GcmResultToError(result));
  SendResponse(gcm::GCMClient::SUCCESS == result);
}

GcmSendFunction::GcmSendFunction() {}

GcmSendFunction::~GcmSendFunction() {}

bool GcmSendFunction::DoWork() {
  scoped_ptr<api::gcm::Send::Params> params(
      api::gcm::Send::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  EXTENSION_FUNCTION_VALIDATE(
      ValidateMessageData(params->message.data.additional_properties));

  gcm::GCMClient::OutgoingMessage outgoing_message;
  outgoing_message.id = params->message.message_id;
  outgoing_message.data = params->message.data.additional_properties;
  if (params->message.time_to_live.get())
    outgoing_message.time_to_live = *params->message.time_to_live;

  GCMProfileService()->Send(
      GetExtension()->id(),
      params->message.destination_id,
      outgoing_message,
      base::Bind(&GcmSendFunction::CompleteFunctionWithResult, this));

  return true;
}

void GcmSendFunction::CompleteFunctionWithResult(
    const std::string& message_id,
    gcm::GCMClient::Result result) {
  SetResult(base::Value::CreateStringValue(message_id));
  SetError(GcmResultToError(result));
  SendResponse(gcm::GCMClient::SUCCESS == result);
}

bool GcmSendFunction::ValidateMessageData(
    const gcm::GCMClient::MessageData& data) const {
  size_t total_size = 0u;
  for (std::map<std::string, std::string>::const_iterator iter = data.begin();
       iter != data.end(); ++iter) {
    total_size += iter->first.size() + iter->second.size();

    if (!IsMessageKeyValid(iter->first) ||
        kMaximumMessageSize < iter->first.size() ||
        kMaximumMessageSize < iter->second.size() ||
        kMaximumMessageSize < total_size)
      return false;
  }

  return total_size != 0;
}

GcmJsEventRouter::GcmJsEventRouter(Profile* profile) : profile_(profile) {}

GcmJsEventRouter::~GcmJsEventRouter() {}

void GcmJsEventRouter::OnMessage(
    const std::string& app_id,
    const gcm::GCMClient::IncomingMessage& message) {
  api::gcm::OnMessage::Message message_arg;
  message_arg.data.additional_properties = message.data;
  if (!message.collapse_key.empty())
    message_arg.collapse_key.reset(new std::string(message.collapse_key));

  scoped_ptr<Event> event(new Event(
      api::gcm::OnMessage::kEventName,
      api::gcm::OnMessage::Create(message_arg).Pass(),
      profile_));
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      app_id, event.Pass());
}

void GcmJsEventRouter::OnMessagesDeleted(const std::string& app_id) {
  scoped_ptr<Event> event(new Event(
      api::gcm::OnMessagesDeleted::kEventName,
      api::gcm::OnMessagesDeleted::Create().Pass(),
      profile_));
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      app_id, event.Pass());
}

void GcmJsEventRouter::OnSendError(const std::string& app_id,
                                   const std::string& message_id,
                                   gcm::GCMClient::Result result) {
  api::gcm::OnSendError::Error error;
  error.message_id.reset(new std::string(message_id));
  error.error_message = GcmResultToError(result);

  scoped_ptr<Event> event(new Event(
      api::gcm::OnSendError::kEventName,
      api::gcm::OnSendError::Create(error).Pass(),
      profile_));
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      app_id, event.Pass());
}

}  // namespace extensions
