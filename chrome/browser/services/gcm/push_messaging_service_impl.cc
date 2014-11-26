// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_service_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/services/gcm/push_messaging_application_id.h"
#include "chrome/browser/services/gcm/push_messaging_constants.h"
#include "chrome/browser/services/gcm/push_messaging_permission_context.h"
#include "chrome/browser/services/gcm/push_messaging_permission_context_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace gcm {

namespace {
const int kMaxRegistrations = 1000000;

blink::WebPushPermissionStatus ToPushPermission(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return blink::WebPushPermissionStatusGranted;
    case CONTENT_SETTING_BLOCK:
      return blink::WebPushPermissionStatusDenied;
    case CONTENT_SETTING_ASK:
      return blink::WebPushPermissionStatusDefault;
    default:
      NOTREACHED();
      return blink::WebPushPermissionStatusDenied;
  }
}

}  // namespace

// static
void PushMessagingServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kPushMessagingRegistrationCount,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
void PushMessagingServiceImpl::InitializeForProfile(Profile* profile) {
  // TODO(mvanouwerkerk): Make sure to remove this check at the same time as
  // push graduates from experimental in Blink.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    return;
  }

  // TODO(johnme): Consider whether push should be enabled in incognito.
  if (!profile || profile->IsOffTheRecord())
    return;

  // TODO(johnme): If push becomes enabled in incognito (and this still uses a
  // pref), be careful that this pref is read from the right profile, as prefs
  // defined in a regular profile are visible in the corresponding incognito
  // profile unless overridden.
  // TODO(johnme): Make sure this pref doesn't get out of sync after crashes.
  int count = profile->GetPrefs()->GetInteger(
      prefs::kPushMessagingRegistrationCount);
  if (count <= 0)
    return;

  // Create the GCMProfileService, and hence instantiate this class.
  GCMProfileService* gcm_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  PushMessagingServiceImpl* push_service =
      static_cast<PushMessagingServiceImpl*>(
          gcm_service->push_messaging_service());

  push_service->IncreasePushRegistrationCount(count);
}

PushMessagingServiceImpl::PushMessagingServiceImpl(
    GCMProfileService* gcm_profile_service,
    Profile* profile)
    : gcm_profile_service_(gcm_profile_service),
      profile_(profile),
      push_registration_count_(0),
      weak_factory_(this) {
}

PushMessagingServiceImpl::~PushMessagingServiceImpl() {
  // TODO(johnme): If it's possible for this to be destroyed before GCMDriver,
  // then we should call RemoveAppHandler.
}

void PushMessagingServiceImpl::IncreasePushRegistrationCount(int add) {
  DCHECK(add > 0);
  if (push_registration_count_ == 0) {
    gcm_profile_service_->driver()->AddAppHandler(
        kPushMessagingApplicationIdPrefix, this);
  }
  push_registration_count_ += add;
  profile_->GetPrefs()->SetInteger(prefs::kPushMessagingRegistrationCount,
                                   push_registration_count_);
}

void PushMessagingServiceImpl::DecreasePushRegistrationCount(int subtract) {
  DCHECK(subtract > 0);
  push_registration_count_ -= subtract;
  DCHECK(push_registration_count_ >= 0);
  profile_->GetPrefs()->SetInteger(prefs::kPushMessagingRegistrationCount,
                                   push_registration_count_);
  if (push_registration_count_ == 0) {
    gcm_profile_service_->driver()->RemoveAppHandler(
        kPushMessagingApplicationIdPrefix);
  }
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return PushMessagingApplicationId::Parse(app_id).IsValid();
}

void PushMessagingServiceImpl::ShutdownHandler() {
  // TODO(johnme): Do any necessary cleanup.
}

void PushMessagingServiceImpl::OnMessage(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  // The Push API only exposes a single string of data in the push event fired
  // on the Service Worker. When developers send messages using GCM to the Push
  // API, they must pass a single key-value pair, where the key is "data" and
  // the value is the string they want to be passed to their Service Worker.
  // For example, they could send the following JSON using the HTTPS GCM API:
  // {
  //     "registration_ids": ["FOO", "BAR"],
  //     "data": {
  //         "data": "BAZ",
  //     },
  //     "delay_while_idle": true,
  // }
  // TODO(johnme): Make sure this is clearly documented for developers.
  PushMessagingApplicationId application_id =
      PushMessagingApplicationId::Parse(app_id);
  DCHECK(application_id.IsValid());
  GCMClient::MessageData::const_iterator it = message.data.find("data");
  if (application_id.IsValid() && it != message.data.end()) {
    const std::string& data = it->second;
    content::BrowserContext::DeliverPushMessage(
        profile_,
        application_id.origin,
        application_id.service_worker_registration_id,
        data,
        base::Bind(&PushMessagingServiceImpl::DeliverMessageCallback,
                   weak_factory_.GetWeakPtr(),
                   application_id,
                   message));
  } else {
    // Drop the message, as it is invalid.
    // TODO(mvanouwerkerk): Show a warning in the developer console of the
    // Service Worker corresponding to app_id.
    // TODO(johnme): Add diagnostic observers (e.g. UMA and an internals page)
    // to know when bad things happen.
  }
}

void PushMessagingServiceImpl::SetProfileForTesting(Profile* profile) {
  profile_ = profile;
}

void PushMessagingServiceImpl::DeliverMessageCallback(
    const PushMessagingApplicationId& application_id,
    const GCMClient::IncomingMessage& message,
    content::PushDeliveryStatus status) {
  // TODO(mvanouwerkerk): UMA logging.
  // TODO(mvanouwerkerk): Is there a way to recover from failure?
}

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {
  // TODO(mvanouwerkerk): Fire push error event on the Service Worker
  // corresponding to app_id.
}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::Register(
    const GURL& origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    int renderer_id,
    int render_frame_id,
    bool user_gesture,
    const content::PushMessagingService::RegisterCallback& callback) {
  if (!gcm_profile_service_->driver()) {
    NOTREACHED() << "There is no GCMDriver. Has GCMProfileService shut down?";
  }

  PushMessagingApplicationId application_id =
      PushMessagingApplicationId(origin, service_worker_registration_id);
  DCHECK(application_id.IsValid());

  if (push_registration_count_ >= kMaxRegistrations) {
    RegisterEnd(callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_LIMIT_REACHED);
    return;
  }

  // If this is registering for the first time then the driver does not have
  // this as an app handler and registration would fail.
  if (gcm_profile_service_->driver()->GetAppHandler(
          kPushMessagingApplicationIdPrefix) != this)
    gcm_profile_service_->driver()->AddAppHandler(
        kPushMessagingApplicationIdPrefix, this);

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(renderer_id, render_frame_id);

  // The frame doesn't exist any more, or we received a bad frame id.
  if (!render_frame_host)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // The page doesn't exist any more or we got a bad render frame host.
  if (!web_contents)
    return;

  // TODO(miguelg) need to send this over IPC when bubble support is
  // implemented.
  int bridge_id = -1;

  const PermissionRequestID id(
      renderer_id, web_contents->GetRoutingID(), bridge_id, GURL());

  GURL embedder = web_contents->GetLastCommittedURL();
  gcm::PushMessagingPermissionContext* permission_context =
      gcm::PushMessagingPermissionContextFactory::GetForProfile(profile_);

  if (permission_context == NULL) {
    RegisterEnd(callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  permission_context->RequestPermission(
      web_contents,
      id,
      embedder,
      user_gesture,
      base::Bind(&PushMessagingServiceImpl::DidRequestPermission,
                 weak_factory_.GetWeakPtr(),
                 application_id,
                 sender_id,
                 callback));
}

blink::WebPushPermissionStatus PushMessagingServiceImpl::GetPermissionStatus(
    const GURL& requesting_origin,
    int renderer_id,
    int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(renderer_id, render_frame_id);

  // The frame doesn't exist any more, or we received a bad frame id.
  if (!render_frame_host)
    return blink::WebPushPermissionStatusDenied;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // The page doesn't exist any more or we got a bad render frame host.
  if (!web_contents)
    return blink::WebPushPermissionStatusDenied;

  GURL embedder_origin = web_contents->GetLastCommittedURL().GetOrigin();
  gcm::PushMessagingPermissionContext* permission_context =
      gcm::PushMessagingPermissionContextFactory::GetForProfile(profile_);
  return ToPushPermission(
      permission_context->GetPermissionStatus(
          requesting_origin, embedder_origin));
}

void PushMessagingServiceImpl::RegisterEnd(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    content::PushRegistrationStatus status) {
  GURL endpoint = GURL(std::string(kPushMessagingEndpoint));
  callback.Run(endpoint, registration_id, status);
  if (status == content::PUSH_REGISTRATION_STATUS_SUCCESS)
    IncreasePushRegistrationCount(1);
}

void PushMessagingServiceImpl::DidRegister(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    GCMClient::Result result) {
  content::PushRegistrationStatus status =
      result == GCMClient::SUCCESS
          ? content::PUSH_REGISTRATION_STATUS_SUCCESS
          : content::PUSH_REGISTRATION_STATUS_SERVICE_ERROR;
  RegisterEnd(callback, registration_id, status);
}

void PushMessagingServiceImpl::DidRequestPermission(
    const PushMessagingApplicationId& application_id,
    const std::string& sender_id,
    const content::PushMessagingService::RegisterCallback& register_callback,
    bool allow) {
  if (!allow) {
    RegisterEnd(register_callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  // The GCMDriver could be NULL if GCMProfileService has been shut down.
  if (!gcm_profile_service_->driver())
    return;

  std::vector<std::string> sender_ids(1, sender_id);

  gcm_profile_service_->driver()->Register(
      application_id.ToString(),
      sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 register_callback));
}

// TODO(johnme): Unregister should call DecreasePushRegistrationCount.

}  // namespace gcm
