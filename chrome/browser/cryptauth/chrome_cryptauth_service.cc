// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cryptauth/chrome_cryptauth_service.h"

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/time/default_clock.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/cryptauth/cryptauth_client.h"
#include "components/cryptauth/cryptauth_client_impl.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enroller.h"
#include "components/cryptauth/cryptauth_enroller_impl.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_enrollment_utils.h"
#include "components/cryptauth/cryptauth_gcm_manager_impl.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/secure_message_delegate.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/prefs/pref_service.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/version_info/version_info.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "base/linux_util.h"
#include "chrome/browser/chromeos/login/easy_unlock/secure_message_delegate_chromeos.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/rect.h"
#endif

namespace {

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : nullptr;
}

std::string GetDeviceId() {
  PrefService* local_state = GetLocalState();
  if (!local_state)
    return std::string();

  std::string device_id = local_state->GetString(prefs::kEasyUnlockDeviceId);
  if (device_id.empty()) {
    device_id = base::GenerateGUID();
    local_state->SetString(prefs::kEasyUnlockDeviceId, device_id);
  }
  return device_id;
}

cryptauth::DeviceClassifier GetDeviceClassifierImpl() {
  cryptauth::DeviceClassifier device_classifier;

#if defined(OS_CHROMEOS)
  int32_t major_version;
  int32_t minor_version;
  int32_t bugfix_version;
  // TODO(tengs): base::OperatingSystemVersionNumbers only works for ChromeOS.
  // We need to get different numbers for other platforms.
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  device_classifier.set_device_os_version_code(major_version);
  device_classifier.set_device_type(cryptauth::CHROME);
#endif

  const std::vector<uint32_t> version_components =
      base::Version(version_info::GetVersionNumber()).components();
  if (!version_components.empty())
    device_classifier.set_device_software_version_code(version_components[0]);

  device_classifier.set_device_software_package(version_info::GetProductName());
  return device_classifier;
}

cryptauth::GcmDeviceInfo GetGcmDeviceInfo() {
  cryptauth::GcmDeviceInfo device_info;
  device_info.set_long_device_id(GetDeviceId());
  device_info.set_device_type(cryptauth::CHROME);
  device_info.set_device_software_version(version_info::GetVersionNumber());
  google::protobuf::int64 software_version_code =
      cryptauth::HashStringToInt64(version_info::GetLastChange());
  device_info.set_device_software_version_code(software_version_code);
  ChromeContentBrowserClient chrome_content_browser_client;
  device_info.set_locale(chrome_content_browser_client.GetApplicationLocale());

#if defined(OS_CHROMEOS)
  device_info.set_device_model(base::SysInfo::GetLsbReleaseBoard());
  device_info.set_device_os_version(base::GetLinuxDistro());
  // The Chrome OS version tracks the Chrome version, so fill in the same value
  // as |device_software_version_code|.
  device_info.set_device_os_version_code(software_version_code);

  // There may not be a Shell instance in tests.
  if (!ash::Shell::HasInstance())
    return device_info;

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  int64_t primary_display_id =
      display_manager->GetPrimaryDisplayCandidate().id();
  display::ManagedDisplayInfo display_info =
      display_manager->GetDisplayInfo(primary_display_id);
  gfx::Rect bounds = display_info.bounds_in_native();

  // TODO(tengs): This is a heuristic to deterimine the DPI of the display, as
  // there is no convenient way of getting this information right now.
  const double dpi = display_info.device_scale_factor() > 1.0f ? 239.0f : 96.0f;
  double width_in_inches = (bounds.width() - bounds.x()) / dpi;
  double height_in_inches = (bounds.height() - bounds.y()) / dpi;
  double diagonal_in_inches = sqrt(width_in_inches * width_in_inches +
                                   height_in_inches * height_in_inches);

  // Note: The unit of this measument is in milli-inches.
  device_info.set_device_display_diagonal_mils(diagonal_in_inches * 1000.0);
#else
// TODO(tengs): Fill in device information for other platforms.
#endif
  return device_info;
}

std::string GetAccountIdImpl(Profile* profile) {
#if defined(OS_CHROMEOS)
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile);
#else
  SigninManager* manager = SigninManagerFactory::GetForProfile(profile);
#endif
  return manager ? manager->GetAuthenticatedAccountId() : std::string();
}

std::unique_ptr<cryptauth::CryptAuthClientFactory>
CreateCryptAuthClientFactoryImpl(Profile* profile) {
  return base::MakeUnique<cryptauth::CryptAuthClientFactoryImpl>(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      GetAccountIdImpl(profile), profile->GetRequestContext(),
      GetDeviceClassifierImpl());
}

std::unique_ptr<cryptauth::SecureMessageDelegate>
CreateSecureMessageDelegateImpl() {
#if defined(OS_CHROMEOS)
  return base::MakeUnique<chromeos::SecureMessageDelegateChromeOS>();
#else
  return nullptr;
#endif
}

class CryptAuthEnrollerFactoryImpl
    : public cryptauth::CryptAuthEnrollerFactory {
 public:
  explicit CryptAuthEnrollerFactoryImpl(Profile* profile) : profile_(profile) {}

  std::unique_ptr<cryptauth::CryptAuthEnroller> CreateInstance() override {
    return base::MakeUnique<cryptauth::CryptAuthEnrollerImpl>(
        CreateCryptAuthClientFactoryImpl(profile_),
        CreateSecureMessageDelegateImpl());
  }

 private:
  Profile* profile_;
};

}  // namespace

// static
std::unique_ptr<ChromeCryptAuthService> ChromeCryptAuthService::Create(
    Profile* profile) {
  std::unique_ptr<cryptauth::CryptAuthGCMManager> gcm_manager =
      base::MakeUnique<cryptauth::CryptAuthGCMManagerImpl>(
          gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
          profile->GetPrefs());

  std::unique_ptr<cryptauth::CryptAuthDeviceManager> device_manager =
      base::MakeUnique<cryptauth::CryptAuthDeviceManager>(
          base::MakeUnique<base::DefaultClock>(),
          CreateCryptAuthClientFactoryImpl(profile), gcm_manager.get(),
          profile->GetPrefs());

  std::unique_ptr<cryptauth::CryptAuthEnrollmentManager> enrollment_manager =
      base::MakeUnique<cryptauth::CryptAuthEnrollmentManager>(
          base::MakeUnique<base::DefaultClock>(),
          base::MakeUnique<CryptAuthEnrollerFactoryImpl>(profile),
          CreateSecureMessageDelegateImpl(), GetGcmDeviceInfo(),
          gcm_manager.get(), profile->GetPrefs());

  // Note: ChromeCryptAuthServiceFactory DependsOn(OAuth2TokenServiceFactory),
  // so |token_service| is guaranteed to outlast this service.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  return base::WrapUnique(new ChromeCryptAuthService(
      std::move(gcm_manager), std::move(device_manager),
      std::move(enrollment_manager), profile, token_service));
}

ChromeCryptAuthService::ChromeCryptAuthService(
    std::unique_ptr<cryptauth::CryptAuthGCMManager> gcm_manager,
    std::unique_ptr<cryptauth::CryptAuthDeviceManager> device_manager,
    std::unique_ptr<cryptauth::CryptAuthEnrollmentManager> enrollment_manager,
    Profile* profile,
    OAuth2TokenService* token_service)
    : KeyedService(),
      cryptauth::CryptAuthService(),
      gcm_manager_(std::move(gcm_manager)),
      enrollment_manager_(std::move(enrollment_manager)),
      device_manager_(std::move(device_manager)),
      profile_(profile),
      token_service_(token_service) {
  gcm_manager_->StartListening();

  if (!token_service_->RefreshTokenIsAvailable(GetAccountId())) {
    PA_LOG(INFO) << "Refresh token not yet available; "
                 << "waiting before starting CryptAuth managers.";
    token_service_->AddObserver(this);
  } else {
    enrollment_manager_->Start();
    device_manager_->Start();
  }
}

ChromeCryptAuthService::~ChromeCryptAuthService() {}

void ChromeCryptAuthService::Shutdown() {
  token_service_->RemoveObserver(this);
  enrollment_manager_.reset();
  device_manager_.reset();
  gcm_manager_.reset();
}

cryptauth::CryptAuthDeviceManager*
ChromeCryptAuthService::GetCryptAuthDeviceManager() {
  return device_manager_.get();
}

cryptauth::CryptAuthEnrollmentManager*
ChromeCryptAuthService::GetCryptAuthEnrollmentManager() {
  return enrollment_manager_.get();
}

cryptauth::DeviceClassifier ChromeCryptAuthService::GetDeviceClassifier() {
  return GetDeviceClassifierImpl();
}

std::string ChromeCryptAuthService::GetAccountId() {
  return GetAccountIdImpl(profile_);
}

std::unique_ptr<cryptauth::SecureMessageDelegate>
ChromeCryptAuthService::CreateSecureMessageDelegate() {
  return CreateSecureMessageDelegateImpl();
}

std::unique_ptr<cryptauth::CryptAuthClientFactory>
ChromeCryptAuthService::CreateCryptAuthClientFactory() {
  return CreateCryptAuthClientFactoryImpl(profile_);
}

void ChromeCryptAuthService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (account_id == GetAccountId()) {
    token_service_->RemoveObserver(this);
    enrollment_manager_->Start();
    device_manager_->Start();
  }
}
