// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_background_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_manual_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_robot_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

// Singleton factory for ArcAuthService.
class ArcAuthServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAuthService,
          ArcAuthServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAuthServiceFactory";

  static ArcAuthServiceFactory* GetInstance() {
    return base::Singleton<ArcAuthServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcAuthServiceFactory>;

  ArcAuthServiceFactory() = default;
  ~ArcAuthServiceFactory() override = default;
};

// Convers mojom::ArcSignInFailureReason into ProvisiningResult.
ProvisioningResult ConvertArcSignInFailureReasonToProvisioningResult(
    mojom::ArcSignInFailureReason reason) {
  using ArcSignInFailureReason = mojom::ArcSignInFailureReason;

#define MAP_PROVISIONING_RESULT(name) \
  case ArcSignInFailureReason::name:  \
    return ProvisioningResult::name

  switch (reason) {
    MAP_PROVISIONING_RESULT(UNKNOWN_ERROR);
    MAP_PROVISIONING_RESULT(MOJO_VERSION_MISMATCH);
    MAP_PROVISIONING_RESULT(MOJO_CALL_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_FAILED);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(GMS_NETWORK_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SERVICE_UNAVAILABLE);
    MAP_PROVISIONING_RESULT(GMS_BAD_AUTHENTICATION);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_FAILED);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_FAILED);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_TIMEOUT);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(NO_NETWORK_CONNECTION);
  }
#undef MAP_PROVISIONING_RESULT

  NOTREACHED() << "unknown reason: " << static_cast<int>(reason);
  return ProvisioningResult::UNKNOWN_ERROR;
}

mojom::ChromeAccountType GetAccountType() {
  return IsArcKioskMode() ? mojom::ChromeAccountType::ROBOT_ACCOUNT
                          : mojom::ChromeAccountType::USER_ACCOUNT;
}

}  // namespace

// static
const char ArcAuthService::kArcServiceName[] = "arc::ArcAuthService";

// static
ArcAuthService* ArcAuthService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAuthServiceFactory::GetForBrowserContext(context);
}

// TODO(lhchavez): Get rid of this class once we can safely remove all the
// deprecated interfaces and only need to care about one type of callback.
class ArcAuthService::AccountInfoNotifier {
 public:
  explicit AccountInfoNotifier(
      const GetAuthCodeDeprecatedCallback& auth_callback)
      : callback_type_(CallbackType::AUTH_CODE),
        auth_callback_(auth_callback) {}

  explicit AccountInfoNotifier(
      const GetAuthCodeAndAccountTypeDeprecatedCallback& auth_account_callback)
      : callback_type_(CallbackType::AUTH_CODE_AND_ACCOUNT),
        auth_account_callback_(auth_account_callback) {}

  explicit AccountInfoNotifier(const AccountInfoCallback& account_info_callback)
      : callback_type_(CallbackType::ACCOUNT_INFO),
        account_info_callback_(account_info_callback) {}

  void Notify(bool is_enforced,
              const std::string& auth_info,
              const std::string& account_name,
              mojom::ChromeAccountType account_type,
              bool is_managed) {
    switch (callback_type_) {
      case CallbackType::AUTH_CODE:
        DCHECK(!auth_callback_.is_null());
        auth_callback_.Run(auth_info, is_enforced);
        break;
      case CallbackType::AUTH_CODE_AND_ACCOUNT:
        DCHECK(!auth_account_callback_.is_null());
        auth_account_callback_.Run(auth_info, is_enforced, account_type);
        break;
      case CallbackType::ACCOUNT_INFO:
        DCHECK(!account_info_callback_.is_null());
        mojom::AccountInfoPtr account_info = mojom::AccountInfo::New();
        account_info->account_name = account_name;
        if (account_type ==
            mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT) {
          account_info->enrollment_token = auth_info;
        } else {
          if (!is_enforced)
            account_info->auth_code = base::nullopt;
          else
            account_info->auth_code = auth_info;
        }
        account_info->account_type = account_type;
        account_info->is_managed = is_managed;
        account_info_callback_.Run(std::move(account_info));
        break;
    }
  }

 private:
  enum class CallbackType { AUTH_CODE, AUTH_CODE_AND_ACCOUNT, ACCOUNT_INFO };

  const CallbackType callback_type_;
  const GetAuthCodeDeprecatedCallback auth_callback_;
  const GetAuthCodeAndAccountTypeDeprecatedCallback auth_account_callback_;
  const AccountInfoCallback account_info_callback_;
};

ArcAuthService::ArcAuthService(content::BrowserContext* browser_context,
                               ArcBridgeService* arc_bridge_service)
    : profile_(Profile::FromBrowserContext(browser_context)),
      arc_bridge_service_(arc_bridge_service),
      binding_(this),
      weak_ptr_factory_(this) {
  arc_bridge_service_->auth()->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  arc_bridge_service_->auth()->RemoveObserver(this);
}

void ArcAuthService::OnInstanceReady() {
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(), Init);
  DCHECK(instance);
  mojom::AuthHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  instance->Init(std::move(host_proxy));
}

void ArcAuthService::OnInstanceClosed() {
  fetcher_.reset();
  notifier_.reset();
}

void ArcAuthService::OnSignInComplete() {
  ArcSessionManager::Get()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
}

void ArcAuthService::OnSignInFailed(mojom::ArcSignInFailureReason reason) {
  ArcSessionManager::Get()->OnProvisioningFinished(
      ConvertArcSignInFailureReasonToProvisioningResult(reason));
}

void ArcAuthService::RequestAccountInfo() {
  RequestAccountInfoInternal(
      base::MakeUnique<ArcAuthService::AccountInfoNotifier>(
          base::Bind(&ArcAuthService::OnAccountInfoReady,
                     weak_ptr_factory_.GetWeakPtr())));
}

void ArcAuthService::ReportMetrics(mojom::MetricsType metrics_type,
                                   int32_t value) {
  switch (metrics_type) {
    case mojom::MetricsType::NETWORK_WAITING_TIME_MILLISECONDS:
      UpdateAuthTiming("ArcAuth.NetworkWaitTime",
                       base::TimeDelta::FromMilliseconds(value));
      break;
    case mojom::MetricsType::CHECKIN_ATTEMPTS:
      UpdateAuthCheckinAttempts(value);
      break;
    case mojom::MetricsType::CHECKIN_TIME_MILLISECONDS:
      UpdateAuthTiming("ArcAuth.CheckinTime",
                       base::TimeDelta::FromMilliseconds(value));
      break;
    case mojom::MetricsType::SIGNIN_TIME_MILLISECONDS:
      UpdateAuthTiming("ArcAuth.SignInTime",
                       base::TimeDelta::FromMilliseconds(value));
      break;
    case mojom::MetricsType::ACCOUNT_CHECK_MILLISECONDS:
      UpdateAuthTiming("ArcAuth.AccountCheckTime",
                       base::TimeDelta::FromMilliseconds(value));
      break;
  }
}

void ArcAuthService::ReportAccountCheckStatus(
    mojom::AccountCheckStatus status) {
  UpdateAuthAccountCheckStatus(status);
}

void ArcAuthService::OnAccountInfoReady(mojom::AccountInfoPtr account_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               OnAccountInfoReady);
  DCHECK(instance);
  instance->OnAccountInfoReady(std::move(account_info));
}

void ArcAuthService::GetAuthCodeDeprecated0(
    const GetAuthCodeDeprecated0Callback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED() << "GetAuthCodeDeprecated0() should no longer be callable";
}

void ArcAuthService::GetAuthCodeDeprecated(
    const GetAuthCodeDeprecatedCallback& callback) {
  // For robot account we must use RequestAccountInfo because it allows
  // to specify account type.
  DCHECK(!IsArcKioskMode());
  RequestAccountInfoInternal(
      base::MakeUnique<ArcAuthService::AccountInfoNotifier>(callback));
}

void ArcAuthService::GetAuthCodeAndAccountTypeDeprecated(
    const GetAuthCodeAndAccountTypeDeprecatedCallback& callback) {
  RequestAccountInfoInternal(
      base::MakeUnique<ArcAuthService::AccountInfoNotifier>(callback));
}

void ArcAuthService::GetIsAccountManagedDeprecated(
    const GetIsAccountManagedDeprecatedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback.Run(policy_util::IsAccountManaged(profile_));
}

void ArcAuthService::RequestAccountInfoInternal(
    std::unique_ptr<AccountInfoNotifier> notifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // No other auth code-related operation may be in progress.
  DCHECK(!notifier_);
  DCHECK(!fetcher_);

  if (IsArcOptInVerificationDisabled()) {
    notifier->Notify(false /* = is_enforced */, std::string() /* auth_info */,
                     std::string() /* auth_name */, GetAccountType(),
                     policy_util::IsAccountManaged(profile_));
    return;
  }

  // Hereafter asynchronous operation. Remember the notifier.
  notifier_ = std::move(notifier);

  if (IsActiveDirectoryUserForProfile(profile_)) {
    // For Active Directory enrolled devices, we get an enrollment token for a
    // managed Google Play account from DMServer.
    auto enrollment_token_fetcher =
        base::MakeUnique<ArcActiveDirectoryEnrollmentTokenFetcher>(
            ArcSessionManager::Get()->support_host());
    enrollment_token_fetcher->Fetch(
        base::Bind(&ArcAuthService::OnEnrollmentTokenFetched,
                   weak_ptr_factory_.GetWeakPtr()));
    fetcher_ = std::move(enrollment_token_fetcher);
    return;
  }
  // For non-AD enrolled devices an auth code is fetched.
  std::unique_ptr<ArcAuthCodeFetcher> auth_code_fetcher;
  if (IsArcKioskMode()) {
    // In Kiosk mode, use Robot auth code fetching.
    auth_code_fetcher = base::MakeUnique<ArcRobotAuthCodeFetcher>();
  } else {
    // Optionally retrieve auth code in silent mode.
    auth_code_fetcher = base::MakeUnique<ArcBackgroundAuthCodeFetcher>(
        profile_, ArcSessionManager::Get()->auth_context());
  }
  auth_code_fetcher->Fetch(base::Bind(&ArcAuthService::OnAuthCodeFetched,
                                      weak_ptr_factory_.GetWeakPtr()));
  fetcher_ = std::move(auth_code_fetcher);
}

void ArcAuthService::OnEnrollmentTokenFetched(
    ArcActiveDirectoryEnrollmentTokenFetcher::Status status,
    const std::string& enrollment_token,
    const std::string& user_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  fetcher_.reset();

  switch (status) {
    case ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS: {
      // Save user_id to the user profile.
      profile_->GetPrefs()->SetString(prefs::kArcActiveDirectoryPlayUserId,
                                      user_id);

      // Send enrollment token to arc.
      notifier_->Notify(true /*is_enforced*/, enrollment_token,
                        std::string() /* account_name */,
                        mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT,
                        true);
      notifier_.reset();
      return;
    }
    case ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE: {
      ArcSessionManager::Get()->OnProvisioningFinished(
          ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
      return;
    }
    case ArcActiveDirectoryEnrollmentTokenFetcher::Status::ARC_DISABLED: {
      ArcSessionManager::Get()->OnProvisioningFinished(
          ProvisioningResult::ARC_DISABLED);
      return;
    }
  }
}

void ArcAuthService::OnAuthCodeFetched(bool success,
                                       const std::string& auth_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  fetcher_.reset();

  if (!success) {
    ArcSessionManager::Get()->OnProvisioningFinished(
        ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
    return;
  }

  notifier_->Notify(!IsArcOptInVerificationDisabled(), auth_code,
                    ArcSessionManager::Get()->auth_context()->full_account_id(),
                    GetAccountType(), policy_util::IsAccountManaged(profile_));
  notifier_.reset();
}

}  // namespace arc
