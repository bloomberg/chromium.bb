// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"

#include <set>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/push_messaging.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace {
const char kChannelIdSeparator[] = "/";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kTokenServiceNotAvailable[] = "Failed to get token service.";
}

namespace extensions {

namespace glue = api::push_messaging;

PushMessagingEventRouter::PushMessagingEventRouter(Profile* profile)
    : profile_(profile) {
}

PushMessagingEventRouter::~PushMessagingEventRouter() {}

void PushMessagingEventRouter::TriggerMessageForTest(
    const std::string& extension_id,
    int subchannel,
    const std::string& payload) {
  OnMessage(extension_id, subchannel, payload);
}

void PushMessagingEventRouter::OnMessage(const std::string& extension_id,
                                         int subchannel,
                                         const std::string& payload) {
  glue::Message message;
  message.subchannel_id = subchannel;
  message.payload = payload;

  scoped_ptr<base::ListValue> args(glue::OnMessage::Create(message));
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_names::kOnPushMessage, args.Pass()));
  event->restrict_to_profile = profile_;
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());
}

// GetChannelId class functions

PushMessagingGetChannelIdFunction::PushMessagingGetChannelIdFunction()
    : interactive_(false) {}

PushMessagingGetChannelIdFunction::~PushMessagingGetChannelIdFunction() {}

bool PushMessagingGetChannelIdFunction::RunImpl() {
  // Fetch the function arguments.
  scoped_ptr<glue::GetChannelId::Params> params(
      glue::GetChannelId::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params && params->interactive) {
    interactive_ = *params->interactive;
  }

  // Balanced in ReportResult()
  AddRef();

  if (!IsUserLoggedIn()) {
    if (interactive_) {
      LoginUIService* login_ui_service =
          LoginUIServiceFactory::GetForProfile(profile());
      login_ui_service->AddObserver(this);
      // OnLoginUICLosed will be called when UI is closed.
      login_ui_service->ShowLoginPopup();
      return true;
    } else {
      error_ = kUserNotSignedIn;
      ReportResult(std::string(), error_);
      return false;
    }
  }

  return StartGaiaIdFetch();
}

bool PushMessagingGetChannelIdFunction::StartGaiaIdFetch() {
  // Start the async fetch of the GAIA ID.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::URLRequestContextGetter* context = profile()->GetRequestContext();
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
  if (!token_service) {
    ReportResult(std::string(), std::string(kTokenServiceNotAvailable));
    return false;
  }
  const std::string& refresh_token =
      token_service->GetOAuth2LoginRefreshToken();
  fetcher_.reset(new ObfuscatedGaiaIdFetcher(context, this, refresh_token));

  // Check the cache, if we already have a gaia ID, use it instead of
  // fetching the ID over the network.
  const std::string& gaia_id =
      token_service->GetTokenForService(GaiaConstants::kObfuscatedGaiaId);
  if (!gaia_id.empty()) {
    ReportResult(gaia_id, std::string());
    return true;
  }

  fetcher_->Start();

  // Will finish asynchronously.
  return true;
}

// Check if the user is logged in.
bool PushMessagingGetChannelIdFunction::IsUserLoggedIn() const {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
  if (!token_service)
    return false;
  return token_service->HasOAuthLoginToken();
}

void PushMessagingGetChannelIdFunction::OnLoginUIShown(
    LoginUIService::LoginUI* ui) {
  // Do nothing when login ui is shown.
}

// If the login succeeds, continue with our logic to fetch the ChannelId.
void PushMessagingGetChannelIdFunction::OnLoginUIClosed(
    LoginUIService::LoginUI* ui) {
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile());
  login_ui_service->RemoveObserver(this);
  if (!StartGaiaIdFetch()) {
    SendResponse(false);
  }
}

void PushMessagingGetChannelIdFunction::ReportResult(
    const std::string& gaia_id, const std::string& error_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BuildAndSendResult(gaia_id, error_string);

  // Cache the obfuscated ID locally. It never changes for this user,
  // and if we call the web API too often, we get errors due to rate limiting.
  if (!gaia_id.empty()) {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
    if (token_service) {
      token_service->AddAuthTokenManually(GaiaConstants::kObfuscatedGaiaId,
                                          gaia_id);
    }
  }

  // Balanced in RunImpl.
  Release();
}

void PushMessagingGetChannelIdFunction::BuildAndSendResult(
    const std::string& gaia_id, const std::string& error_message) {
  std::string channel_id;
  if (!gaia_id.empty()) {
    channel_id = gaia_id;
    channel_id += kChannelIdSeparator;
    channel_id += extension_id();
  }

  // TODO(petewil): It may be a good idea to further
  // obfuscate the channel ID to prevent the user's obfuscated GAIA ID
  // from being readily obtained.  Security review will tell us if we need to.

  // Create a ChannelId results object and set the fields.
  glue::ChannelIdResult result;
  result.channel_id = channel_id;
  SetError(error_message);
  results_ = glue::GetChannelId::Results::Create(result);

  bool success = error_message.empty() && !gaia_id.empty();
  SendResponse(success);
}

void PushMessagingGetChannelIdFunction::OnObfuscatedGaiaIdFetchSuccess(
    const std::string& gaia_id) {
  ReportResult(gaia_id, std::string());
}

void PushMessagingGetChannelIdFunction::OnObfuscatedGaiaIdFetchFailure(
      const GoogleServiceAuthError& error) {
  std::string error_text = error.error_message();
  // If the error message is blank, see if we can set it from the state.
  if (error_text.empty() &&
      (0 != error.state())) {
    error_text = base::IntToString(error.state());
  }

  ReportResult(std::string(), error_text);
}

PushMessagingAPI::PushMessagingAPI(Profile* profile) : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

PushMessagingAPI::~PushMessagingAPI() {
}

// static
PushMessagingAPI* PushMessagingAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<PushMessagingAPI>::GetForProfile(profile);
}

void PushMessagingAPI::Shutdown() {
  event_router_.reset();
  handler_.reset();
}

static base::LazyInstance<ProfileKeyedAPIFactory<PushMessagingAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<PushMessagingAPI>*
PushMessagingAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void PushMessagingAPI::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  ProfileSyncService* pss = ProfileSyncServiceFactory::GetForProfile(profile_);
  // This may be NULL; for example, for the ChromeOS guest user. In these cases,
  // just return without setting up anything, since it won't work anyway.
  if (!pss)
    return;

  if (!event_router_)
    event_router_.reset(new PushMessagingEventRouter(profile_));
  if (!handler_) {
    handler_.reset(new PushMessagingInvalidationHandler(
        pss, event_router_.get()));
  }
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
        handler_->RegisterExtension(extension->id());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
        handler_->UnregisterExtension(extension->id());
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void PushMessagingAPI::SetMapperForTest(
    scoped_ptr<PushMessagingInvalidationMapper> mapper) {
  handler_ = mapper.Pass();
}

template <>
void ProfileKeyedAPIFactory<PushMessagingAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

}  // namespace extensions
