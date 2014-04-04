// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"

#include <set>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/token_cache/token_cache_service.h"
#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"
#include "chrome/browser/invalidation/invalidation_auth_provider.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/push_messaging.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "google_apis/gaia/gaia_constants.h"

using content::BrowserThread;

const char kChannelIdSeparator[] = "/";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kUserAccessTokenFailure[] =
    "Cannot obtain access token for the user.";
const char kAPINotAvailableForUser[] =
    "The API is not available for this user.";
const int kObfuscatedGaiaIdTimeoutInDays = 30;

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

  DVLOG(2) << "PushMessagingEventRouter::OnMessage"
           << " payload = '" << payload
           << "' subchannel = '" << subchannel
           << "' extension = '" << extension_id << "'";

  scoped_ptr<base::ListValue> args(glue::OnMessage::Create(message));
  scoped_ptr<extensions::Event> event(new extensions::Event(
      glue::OnMessage::kEventName, args.Pass()));
  event->restrict_to_browser_context = profile_;
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());
}

// GetChannelId class functions

PushMessagingGetChannelIdFunction::PushMessagingGetChannelIdFunction()
    : OAuth2TokenService::Consumer("push_messaging"),
      interactive_(false) {}

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

  invalidation::InvalidationService* invalidation_service =
      invalidation::InvalidationServiceFactory::GetForProfile(GetProfile());
  if (!invalidation_service) {
    error_ = kAPINotAvailableForUser;
    ReportResult(std::string(), error_);
    return false;
  }

  invalidation::InvalidationAuthProvider* auth_provider =
      invalidation_service->GetInvalidationAuthProvider();
  if (!auth_provider->GetTokenService()->RefreshTokenIsAvailable(
          auth_provider->GetAccountId())) {
    if (interactive_ && auth_provider->ShowLoginUI()) {
      auth_provider->GetTokenService()->AddObserver(this);
      return true;
    } else {
      error_ = kUserNotSignedIn;
      ReportResult(std::string(), error_);
      return false;
    }
  }

  DVLOG(2) << "Logged in profile name: " << GetProfile()->GetProfileName();

  StartAccessTokenFetch();
  return true;
}

void PushMessagingGetChannelIdFunction::StartAccessTokenFetch() {
  invalidation::InvalidationService* invalidation_service =
      invalidation::InvalidationServiceFactory::GetForProfile(GetProfile());
  CHECK(invalidation_service);
  invalidation::InvalidationAuthProvider* auth_provider =
      invalidation_service->GetInvalidationAuthProvider();

  std::vector<std::string> scope_vector =
      extensions::ObfuscatedGaiaIdFetcher::GetScopes();
  OAuth2TokenService::ScopeSet scopes(scope_vector.begin(), scope_vector.end());
  fetcher_access_token_request_ =
      auth_provider->GetTokenService()->StartRequest(
          auth_provider->GetAccountId(), scopes, this);
}

void PushMessagingGetChannelIdFunction::OnRefreshTokenAvailable(
    const std::string& account_id) {
  invalidation::InvalidationService* invalidation_service =
      invalidation::InvalidationServiceFactory::GetForProfile(GetProfile());
  CHECK(invalidation_service);
  invalidation_service->GetInvalidationAuthProvider()->GetTokenService()->
      RemoveObserver(this);
  DVLOG(2) << "Newly logged in: " << GetProfile()->GetProfileName();
  StartAccessTokenFetch();
}

void PushMessagingGetChannelIdFunction::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(fetcher_access_token_request_.get(), request);
  fetcher_access_token_request_.reset();

  StartGaiaIdFetch(access_token);
}

void PushMessagingGetChannelIdFunction::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(fetcher_access_token_request_.get(), request);
  fetcher_access_token_request_.reset();

  // TODO(fgorski): We are currently ignoring the error passed in upon failure.
  // It should be revisited when we are working on improving general error
  // handling for the identity related code.
  DVLOG(1) << "Cannot obtain access token for this user "
           << error.error_message() << " " << error.state();
  error_ = kUserAccessTokenFailure;
  ReportResult(std::string(), error_);
}

void PushMessagingGetChannelIdFunction::StartGaiaIdFetch(
    const std::string& access_token) {
  // Start the async fetch of the Gaia Id.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::URLRequestContextGetter* context = GetProfile()->GetRequestContext();
  fetcher_.reset(new ObfuscatedGaiaIdFetcher(context, this, access_token));

  // Get the token cache and see if we have already cached a Gaia Id.
  TokenCacheService* token_cache =
      TokenCacheServiceFactory::GetForProfile(GetProfile());

  // Check the cache, if we already have a Gaia ID, use it instead of
  // fetching the ID over the network.
  const std::string& gaia_id =
      token_cache->RetrieveToken(GaiaConstants::kObfuscatedGaiaId);
  if (!gaia_id.empty()) {
    ReportResult(gaia_id, std::string());
    return;
  }

  fetcher_->Start();
}

void PushMessagingGetChannelIdFunction::ReportResult(
    const std::string& gaia_id, const std::string& error_string) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BuildAndSendResult(gaia_id, error_string);

  // Cache the obfuscated ID locally. It never changes for this user,
  // and if we call the web API too often, we get errors due to rate limiting.
  if (!gaia_id.empty()) {
    base::TimeDelta timeout =
        base::TimeDelta::FromDays(kObfuscatedGaiaIdTimeoutInDays);
    TokenCacheService* token_cache =
        TokenCacheServiceFactory::GetForProfile(GetProfile());
    token_cache->StoreToken(GaiaConstants::kObfuscatedGaiaId, gaia_id,
                            timeout);
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
  // obfuscate the channel ID to prevent the user's obfuscated Gaia Id
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

  DVLOG(1) << "GetChannelId status: '" << error_text << "'";

  // If we had bad credentials, try the logon again.
  switch (error.state()) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED: {
      invalidation::InvalidationService* invalidation_service =
          invalidation::InvalidationServiceFactory::GetForProfile(GetProfile());
      CHECK(invalidation_service);
      if (!interactive_ ||
          !invalidation_service->GetInvalidationAuthProvider()->ShowLoginUI()) {
        ReportResult(std::string(), error_text);
      }
      return;
    }
    default:
      // Return error to caller.
      ReportResult(std::string(), error_text);
      return;
  }
}

PushMessagingAPI::PushMessagingAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

PushMessagingAPI::~PushMessagingAPI() {
}

// static
PushMessagingAPI* PushMessagingAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PushMessagingAPI>::Get(context);
}

void PushMessagingAPI::Shutdown() {
  event_router_.reset();
  handler_.reset();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<PushMessagingAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<PushMessagingAPI>*
PushMessagingAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void PushMessagingAPI::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  invalidation::InvalidationService* invalidation_service =
      invalidation::InvalidationServiceFactory::GetForProfile(profile_);
  if (!invalidation_service)
    return;

  if (!event_router_)
    event_router_.reset(new PushMessagingEventRouter(profile_));
  if (!handler_) {
    handler_.reset(new PushMessagingInvalidationHandler(
        invalidation_service, event_router_.get()));
  }
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
        handler_->SuppressInitialInvalidationsForExtension(extension->id());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
        handler_->RegisterExtension(extension->id());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
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
void
BrowserContextKeyedAPIFactory<PushMessagingAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(invalidation::InvalidationServiceFactory::GetInstance());
}

}  // namespace extensions
