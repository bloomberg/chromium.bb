// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/crd_host_delegate.h"

#include "base/json/json_reader.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

const char kICEConfigURL[] =
    "https://www.googleapis.com/chromoting/v1/@me/iceconfig";

// OAuth2 Token scopes
constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting";

net::NetworkTrafficAnnotationTag CreateIceConfigRequestAnnotation() {
  return net::DefineNetworkTrafficAnnotation("CRD_ice_config_request", R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request is used by Chrome Remote Desktop to fetch ICE "
            "configuration which contains list of STUN & TURN servers and TURN "
            "credentials."
          trigger:
            "When a Chrome Remote Desktop session is being connected and "
            "periodically while a session is active, as necessary. Currently "
            "the API issues credentials that expire every 24 hours, so this "
            "request will only be sent again while session is active more than "
            "24 hours and it needs to renegotiate the ICE connection. The 24 "
            "hour period is controlled by the server and may change. In some "
            "cases, e.g. if direct connection is used, it will not trigger "
            "periodically."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings. You can block Chrome "
            "Remote Desktop as specified here: "
            "https://support.google.com/chrome/?p=remote_desktop"
          chrome_policy {
            RemoteAccessHostFirewallTraversal {
              policy_options {mode: MANDATORY}
              RemoteAccessHostFirewallTraversal: false
            }
          }
        }
        comments:
          "Above specified policy is only applicable on the host side and "
          "doesn't have effect in Android and iOS client apps. The product "
          "is shipped separately from Chromium, except on Chrome OS."
        )");
}

}  // namespace

CRDHostDelegate::CRDHostDelegate()
    : OAuth2TokenService::Consumer("crd_host_delegate"), weak_factory_(this) {}

CRDHostDelegate::~CRDHostDelegate() {
  // TODO(antrim): shutdown host somewhat correctly.
}

bool CRDHostDelegate::HasActiveSession() const {
  return false;
}

void CRDHostDelegate::TerminateSession(base::OnceClosure callback) {
  std::move(callback).Run();
}

bool CRDHostDelegate::AreServicesReady() const {
  return user_manager::UserManager::IsInitialized() &&
         ui::UserActivityDetector::Get() != nullptr &&
         chromeos::ProfileHelper::Get() != nullptr &&
         chromeos::DeviceOAuth2TokenServiceFactory::Get() != nullptr;
}

bool CRDHostDelegate::IsRunningKiosk() const {
  auto* user_manager = user_manager::UserManager::Get();
  // TODO(antrim): find out if Arc Kiosk is also eligible.
  // TODO(antrim): find out if only auto-started Kiosks are elidgible.
  return user_manager->IsLoggedInAsKioskApp() && GetKioskProfile() != nullptr;
}

base::TimeDelta CRDHostDelegate::GetIdlenessPeriod() const {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

void CRDHostDelegate::FetchOAuthToken(
    DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!oauth_success_callback_);
  DCHECK(!error_callback_);
  chromeos::DeviceOAuth2TokenService* oauth_service =
      chromeos::DeviceOAuth2TokenServiceFactory::Get();

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  scopes.insert(GaiaConstants::kGoogleTalkOAuth2Scope);
  scopes.insert(kCloudDevicesOAuth2Scope);
  scopes.insert(kChromotingOAuth2Scope);

  oauth_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  oauth_request_ = oauth_service->StartRequest(
      oauth_service->GetRobotAccountId(), scopes, this);
}

void CRDHostDelegate::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  oauth_request_.reset();
  error_callback_.Reset();
  std::move(oauth_success_callback_).Run(access_token);
}

void CRDHostDelegate::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  oauth_request_.reset();
  oauth_success_callback_.Reset();
  std::move(error_callback_)
      .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
           error.ToString());
}

void CRDHostDelegate::FetchICEConfig(
    const std::string& oauth_token,
    DeviceCommandStartCRDSessionJob::ICEConfigCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!ice_success_callback_);
  DCHECK(!error_callback_);

  ice_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  auto ice_request = std::make_unique<network::ResourceRequest>();
  ice_request->url = GURL(kICEConfigURL);
  ice_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;

  ice_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 "Bearer " + oauth_token);
  auto loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(GetKioskProfile())
          ->GetURLLoaderFactoryForBrowserProcess();

  ice_config_loader_ = network::SimpleURLLoader::Create(
      std::move(ice_request), CreateIceConfigRequestAnnotation());
  ice_config_loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&CRDHostDelegate::OnICEConfigurationLoaded,
                     weak_factory_.GetWeakPtr()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void CRDHostDelegate::OnICEConfigurationLoaded(
    std::unique_ptr<std::string> response_body) {
  ice_config_loader_.reset();
  if (response_body) {
    std::unique_ptr<base::Value> value = base::JSONReader::Read(*response_body);
    if (!value || !value->is_dict()) {
      ice_success_callback_.Reset();
      std::move(error_callback_)
          .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_ICE_CONFIG,
               "Could not parse config");
      return;
    }
    error_callback_.Reset();
    std::move(ice_success_callback_).Run(std::move(*value));
  } else {
    ice_success_callback_.Reset();
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_ICE_CONFIG,
             std::string());
  }
}

void CRDHostDelegate::StartCRDHostAndGetCode(
    const std::string& directory_bot_jid,
    const std::string& oauth_token,
    base::Value ice_config,
    DeviceCommandStartCRDSessionJob::AuthCodeCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  // TODO(antrim): actual implementation
  std::move(success_callback).Run(std::string("TODO: Auth Code"));
}

Profile* CRDHostDelegate::GetKioskProfile() const {
  auto* user_manager = user_manager::UserManager::Get();
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->GetActiveUser());
}

}  // namespace policy
