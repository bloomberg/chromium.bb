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
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/token_cache/token_cache_service.h"
#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/push_messaging.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/identity_provider.h"

using content::BrowserThread;

namespace extensions {

namespace {
const char kChannelIdSeparator[] = "/";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kUserAccessTokenFailure[] =
    "Cannot obtain access token for the user.";
const char kAPINotAvailableForUser[] =
    "The API is not available for this user.";
const int kObfuscatedGaiaIdTimeoutInDays = 30;
}

namespace glue = api::push_messaging;

PushMessagingEventRouter::PushMessagingEventRouter(
    content::BrowserContext* context)
    : browser_context_(context) {
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
  scoped_ptr<Event> event(new Event(glue::OnMessage::kEventName, args.Pass()));
  event->restrict_to_browser_context = browser_context_;
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, event.Pass());
}

// GetChannelId class functions

PushMessagingGetChannelIdFunction::PushMessagingGetChannelIdFunction()
    : OAuth2TokenService::Consumer("push_messaging"),
      interactive_(false) {}

PushMessagingGetChannelIdFunction::~PushMessagingGetChannelIdFunction() {}

bool PushMessagingGetChannelIdFunction::RunAsync() {
  // Fetch the function arguments.
  scoped_ptr<glue::GetChannelId::Params> params(
      glue::GetChannelId::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params && params->interactive) {
    interactive_ = *params->interactive;
  }

  // Balanced in ReportResult()
  AddRef();

  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          GetProfile());
  if (!invalidation_provider) {
    error_ = kAPINotAvailableForUser;
    ReportResult(std::string(), error_);
    return false;
  }

  IdentityProvider* identity_provider =
      invalidation_provider->GetInvalidationService()->GetIdentityProvider();
  if (!identity_provider->GetTokenService()->RefreshTokenIsAvailable(
          identity_provider->GetActiveAccountId())) {
    if (interactive_ && identity_provider->RequestLogin()) {
      identity_provider->AddActiveAccountRefreshTokenObserver(this);
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
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          GetProfile());
  CHECK(invalidation_provider);
  IdentityProvider* identity_provider =
      invalidation_provider->GetInvalidationService()->GetIdentityProvider();

  std::vector<std::string> scope_vector = ObfuscatedGaiaIdFetcher::GetScopes();
  OAuth2TokenService::ScopeSet scopes(scope_vector.begin(), scope_vector.end());
  fetcher_access_token_request_ =
      identity_provider->GetTokenService()->StartRequest(
          identity_provider->GetActiveAccountId(), scopes, this);
}

void PushMessagingGetChannelIdFunction::OnRefreshTokenAvailable(
    const std::string& account_id) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          GetProfile());
  CHECK(invalidation_provider);
  invalidation_provider->GetInvalidationService()->GetIdentityProvider()->
      RemoveActiveAccountRefreshTokenObserver(this);
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

  // Balanced in RunAsync.
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
      invalidation::ProfileInvalidationProvider* invalidation_provider =
          invalidation::ProfileInvalidationProviderFactory::GetForProfile(
              GetProfile());
      CHECK(invalidation_provider);
      if (!interactive_ || !invalidation_provider->GetInvalidationService()->
              GetIdentityProvider()->RequestLogin()) {
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
    : extension_registry_observer_(this), browser_context_(context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
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

bool PushMessagingAPI::InitEventRouterAndHandler() {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));
  if (!invalidation_provider)
    return false;

  if (!event_router_)
    event_router_.reset(new PushMessagingEventRouter(browser_context_));
  if (!handler_) {
    handler_.reset(new PushMessagingInvalidationHandler(
        invalidation_provider->GetInvalidationService(),
        event_router_.get()));
  }

  return true;
}

void PushMessagingAPI::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!InitEventRouterAndHandler())
    return;

  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kPushMessaging)) {
    handler_->RegisterExtension(extension->id());
  }
}

void PushMessagingAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (!InitEventRouterAndHandler())
    return;

  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kPushMessaging)) {
    handler_->UnregisterExtension(extension->id());
  }
}

void PushMessagingAPI::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  if (InitEventRouterAndHandler() &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kPushMessaging)) {
    handler_->SuppressInitialInvalidationsForExtension(extension->id());
  }
}

void PushMessagingAPI::SetMapperForTest(
    scoped_ptr<PushMessagingInvalidationMapper> mapper) {
  handler_ = mapper.Pass();
}

template <>
void
BrowserContextKeyedAPIFactory<PushMessagingAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
}

}  // namespace extensions
