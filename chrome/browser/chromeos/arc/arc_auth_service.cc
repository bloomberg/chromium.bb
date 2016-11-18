// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/auth/arc_robot_auth.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_features.h"
#include "content/public/browser/browser_thread.h"

namespace arc {
namespace {

ArcAuthService* g_arc_auth_service = nullptr;

constexpr uint32_t kMinVersionForOnAccountInfoReady = 5;

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
  }
#undef MAP_PROVISIONING_RESULT

  NOTREACHED() << "unknown reason: " << static_cast<int>(reason);
  return ProvisioningResult::UNKNOWN_ERROR;
}

mojom::ChromeAccountType GetAccountType() {
  return ArcSessionManager::IsArcKioskMode()
             ? mojom::ChromeAccountType::ROBOT_ACCOUNT
             : mojom::ChromeAccountType::USER_ACCOUNT;
}

}  // namespace

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
              const std::string& auth_code,
              mojom::ChromeAccountType account_type,
              bool is_managed) {
    switch (callback_type_) {
      case CallbackType::AUTH_CODE:
        DCHECK(!auth_callback_.is_null());
        auth_callback_.Run(auth_code, is_enforced);
        break;
      case CallbackType::AUTH_CODE_AND_ACCOUNT:
        DCHECK(!auth_account_callback_.is_null());
        auth_account_callback_.Run(auth_code, is_enforced, account_type);
        break;
      case CallbackType::ACCOUNT_INFO:
        DCHECK(!account_info_callback_.is_null());
        mojom::AccountInfoPtr account_info = mojom::AccountInfo::New();
        if (!is_enforced) {
          account_info->auth_code = base::nullopt;
        } else {
          account_info->auth_code = auth_code;
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

ArcAuthService::ArcAuthService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_ptr_factory_(this) {
  DCHECK(!g_arc_auth_service);
  g_arc_auth_service = this;
  arc_bridge_service()->auth()->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  arc_bridge_service()->auth()->RemoveObserver(this);

  DCHECK_EQ(g_arc_auth_service, this);
  g_arc_auth_service = nullptr;
}

// static
ArcAuthService* ArcAuthService::GetForTest() {
  DCHECK(g_arc_auth_service);
  return g_arc_auth_service;
}

void ArcAuthService::OnInstanceReady() {
  auto* instance = arc_bridge_service()->auth()->GetInstanceForMethod("Init");
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcAuthService::OnInstanceClosed() {
  ArcSupportHost* support_host = ArcSessionManager::Get()->support_host();
  if (support_host)
    support_host->RemoveObserver(this);
  notifier_.reset();
  arc_robot_auth_.reset();
  auth_code_fetcher_.reset();
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

void ArcAuthService::GetAuthCodeDeprecated0(
    const GetAuthCodeDeprecated0Callback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED() << "GetAuthCodeDeprecated0() should no longer be callable";
}

void ArcAuthService::GetAuthCodeDeprecated(
    const GetAuthCodeDeprecatedCallback& callback) {
  // For robot account we must use RequestAccountInfo because it allows
  // to specify account type.
  DCHECK(!ArcSessionManager::IsArcKioskMode());
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
  callback.Run(
      policy_util::IsAccountManaged(ArcSessionManager::Get()->profile()));
}

void ArcAuthService::RequestAccountInfoInternal(
    std::unique_ptr<AccountInfoNotifier> notifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // No other auth code-related operation may be in progress.
  DCHECK(!notifier_);

  if (ArcSessionManager::IsOptInVerificationDisabled()) {
    notifier->Notify(
        false /* = is_enforced */, std::string(), GetAccountType(),
        policy_util::IsAccountManaged(ArcSessionManager::Get()->profile()));
    return;
  }

  // Hereafter asynchronous operation. Remember the notifier.
  notifier_ = std::move(notifier);

  // In Kiosk mode, use Robot auth code fetching.
  if (ArcSessionManager::IsArcKioskMode()) {
    arc_robot_auth_.reset(new ArcRobotAuth());
    arc_robot_auth_->FetchRobotAuthCode(
        base::Bind(&ArcAuthService::OnRobotAuthCodeFetched,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Optionally retrive auth code in silent mode.
  if (base::FeatureList::IsEnabled(arc::kArcUseAuthEndpointFeature)) {
    DCHECK(!auth_code_fetcher_);
    auth_code_fetcher_ = base::MakeUnique<ArcAuthCodeFetcher>(
        ArcSessionManager::Get()->profile(),
        ArcSessionManager::Get()->auth_context());
    auth_code_fetcher_->Fetch(base::Bind(&ArcAuthService::OnAuthCodeFetched,
                                         weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Otherwise, show LSO page to user after HTTP context preparation, and let
  // them click "Sign in" button.
  ArcSessionManager::Get()->auth_context()->Prepare(base::Bind(
      &ArcAuthService::OnContextPrepared, weak_ptr_factory_.GetWeakPtr()));
}

void ArcAuthService::OnContextPrepared(
    net::URLRequestContextGetter* request_context_getter) {
  ArcSupportHost* support_host = ArcSessionManager::Get()->support_host();
  // Here, support_host should be available always. The case support_host is
  // not created is when 1) IsOptInVerificationDisabled() is true or 2)
  // IsArcKioskMode() is true. Both cases are handled above.
  DCHECK(support_host);
  if (!support_host->HasObserver(this))
    support_host->AddObserver(this);

  if (request_context_getter) {
    support_host->ShowLso();
  } else {
    support_host->ShowError(ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR,
                            false);
  }
}

void ArcAuthService::OnAccountInfoReady(mojom::AccountInfoPtr account_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* instance = arc_bridge_service()->auth()->GetInstanceForMethod(
      "OnAccountInfoReady", kMinVersionForOnAccountInfoReady);
  DCHECK(instance);
  instance->OnAccountInfoReady(std::move(account_info));
}

void ArcAuthService::OnRobotAuthCodeFetched(
    const std::string& robot_auth_code) {
  // We fetching robot auth code for ARC kiosk only.
  DCHECK(ArcSessionManager::IsArcKioskMode());

  // Current instance of ArcRobotAuth became useless.
  arc_robot_auth_.reset();

  if (robot_auth_code.empty()) {
    VLOG(1) << "Robot account auth code fetching error";
    // Log out the user. All the cleanup will be done in Shutdown() method.
    // The callback is not called because auth code is empty.
    chrome::AttemptUserExit();
    return;
  }

  OnAuthCodeObtained(robot_auth_code);
}

void ArcAuthService::OnAuthCodeFetched(const std::string& auth_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auth_code_fetcher_.reset();

  if (auth_code.empty()) {
    ArcSessionManager::Get()->OnProvisioningFinished(
        ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
    return;
  }

  OnAuthCodeObtained(auth_code);
}

void ArcAuthService::OnAuthCodeObtained(const std::string& auth_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!auth_code.empty());

  notifier_->Notify(
      !ArcSessionManager::IsOptInVerificationDisabled(), auth_code,
      GetAccountType(),
      policy_util::IsAccountManaged(ArcSessionManager::Get()->profile()));
  notifier_.reset();
}

void ArcAuthService::OnAuthSucceeded(const std::string& auth_code) {
  OnAuthCodeObtained(auth_code);
}

void ArcAuthService::OnRetryClicked() {
  ArcSupportHost* support_host = ArcSessionManager::Get()->support_host();
  // This is the callback for the UI event, so support_host should be always
  // available here.
  DCHECK(support_host);
  if (support_host->ui_page() == ArcSupportHost::UIPage::ERROR) {
    // This case is handled by ArcSessionManager::OnRetryClicked().
    return;
  }

  ArcSessionManager::Get()->auth_context()->Prepare(base::Bind(
      &ArcAuthService::OnContextPrepared, weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace arc
