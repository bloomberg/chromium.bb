// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "policy/proto/device_management_backend.pb.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace {

// Desktop notification constants.
const char kEnrollmentNotificationId[] = "consumer_management.enroll";
const char kEnrollmentNotificationUrl[] = "chrome://consumer-management/enroll";

// The path to the consumer management enrollment/unenrollment confirmation
// overlay, relative to the settings page URL.
const char kConsumerManagementOverlay[] = "consumer-management-overlay";

  // Returns the account ID signed in to |profile|.
const std::string& GetAccountIdFromProfile(Profile* profile) {
  return SigninManagerFactory::GetForProfile(profile)->
      GetAuthenticatedAccountId();
}

class DesktopNotificationDelegate : public NotificationDelegate {
 public:
  // |button_click_callback| is called when the button in the notification is
  // clicked.
  DesktopNotificationDelegate(const std::string& id,
                              const base::Closure& button_click_callback);

  // NotificationDelegate:
  virtual std::string id() const override;
  virtual void ButtonClick(int button_index) override;

 private:
  virtual ~DesktopNotificationDelegate();

  std::string id_;
  base::Closure button_click_callback_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationDelegate);
};

DesktopNotificationDelegate::DesktopNotificationDelegate(
    const std::string& id,
    const base::Closure& button_click_callback)
    : id_(id), button_click_callback_(button_click_callback) {
}

DesktopNotificationDelegate::~DesktopNotificationDelegate() {
}

std::string DesktopNotificationDelegate::id() const {
  return id_;
}

void DesktopNotificationDelegate::ButtonClick(int button_index) {
  button_click_callback_.Run();
}

}  // namespace

namespace policy {

ConsumerEnrollmentHandler::ConsumerEnrollmentHandler(
    ConsumerManagementService* consumer_management_service,
    DeviceManagementService* device_management_service)
    : Consumer("consumer_enrollment_handler"),
      consumer_management_service_(consumer_management_service),
      device_management_service_(device_management_service),
      enrolling_profile_(NULL),
      weak_ptr_factory_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
}

ConsumerEnrollmentHandler::~ConsumerEnrollmentHandler() {
  if (enrolling_profile_) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(enrolling_profile_)->
        RemoveObserver(this);
  }
  registrar_.Remove(this,
                    chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                    content::NotificationService::AllSources());
}

void ConsumerEnrollmentHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED) {
    NOTREACHED() << "Unexpected notification " << type;
    return;
  }

  Profile* profile = content::Details<Profile>(details).ptr();
  if (chromeos::ProfileHelper::IsOwnerProfile(profile))
    OnOwnerSignin(profile);
}

void ConsumerEnrollmentHandler::OnRefreshTokenAvailable(
    const std::string& account_id) {
  CHECK(enrolling_profile_);

  if (account_id == GetAccountIdFromProfile(enrolling_profile_)) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(enrolling_profile_)->
        RemoveObserver(this);
    OnOwnerRefreshTokenAvailable();
  }
}

void ConsumerEnrollmentHandler::OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) {
  DCHECK_EQ(token_request_, request);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, token_request_.release());

  OnOwnerAccessTokenAvailable(access_token);
}

void ConsumerEnrollmentHandler::OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) {
  DCHECK_EQ(token_request_, request);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, token_request_.release());

  LOG(ERROR) << "Failed to get the access token: " << error.ToString();
  EndEnrollment(ConsumerManagementService::ENROLLMENT_STAGE_GET_TOKEN_FAILED);
}

void ConsumerEnrollmentHandler::OnOwnerSignin(Profile* profile) {
  const ConsumerManagementService::EnrollmentStage stage =
      consumer_management_service_->GetEnrollmentStage();
  switch (stage) {
    case ConsumerManagementService::ENROLLMENT_STAGE_NONE:
      // Do nothing.
      return;

    case ConsumerManagementService::ENROLLMENT_STAGE_OWNER_STORED:
      // Continue the enrollment process after the owner signs in.
      ContinueEnrollmentProcess(profile);
      return;

    case ConsumerManagementService::ENROLLMENT_STAGE_SUCCESS:
    case ConsumerManagementService::ENROLLMENT_STAGE_CANCELED:
    case ConsumerManagementService::ENROLLMENT_STAGE_BOOT_LOCKBOX_FAILED:
    case ConsumerManagementService::ENROLLMENT_STAGE_DM_SERVER_FAILED:
    case ConsumerManagementService::ENROLLMENT_STAGE_GET_TOKEN_FAILED:
      ShowDesktopNotificationAndResetStage(stage, profile);
      return;

    case ConsumerManagementService::ENROLLMENT_STAGE_REQUESTED:
    case ConsumerManagementService::ENROLLMENT_STAGE_LAST:
      NOTREACHED() << "Unexpected enrollment stage " << stage;
      return;
  }
}

void ConsumerEnrollmentHandler::ContinueEnrollmentProcess(Profile* profile) {
  enrolling_profile_ = profile;

  // First, we need to ensure that the refresh token is available.
  const std::string& account_id = GetAccountIdFromProfile(profile);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (token_service->RefreshTokenIsAvailable(account_id)) {
    OnOwnerRefreshTokenAvailable();
  } else {
    token_service->AddObserver(this);
  }
}

void ConsumerEnrollmentHandler::OnOwnerRefreshTokenAvailable() {
  CHECK(enrolling_profile_);

  // Now we can request the OAuth access token for device management to send the
  // device registration request to the device management server.
  OAuth2TokenService::ScopeSet oauth_scopes;
  oauth_scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  const std::string& account_id = GetAccountIdFromProfile(enrolling_profile_);
  token_request_ = ProfileOAuth2TokenServiceFactory::GetForProfile(
      enrolling_profile_)->StartRequest(account_id, oauth_scopes, this);
}

void ConsumerEnrollmentHandler::OnOwnerAccessTokenAvailable(
    const std::string& access_token) {
  // Now that we have the access token, we got everything we need to send the
  // device registration request to the device management server.
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceCloudPolicyInitializer* initializer =
      connector->GetDeviceCloudPolicyInitializer();
  CHECK(initializer);

  policy::DeviceCloudPolicyInitializer::AllowedDeviceModes device_modes;
  device_modes[policy::DEVICE_MODE_ENTERPRISE] = true;

  initializer->StartEnrollment(
      em::PolicyData::CONSUMER_MANAGED,
      device_management_service_,
      access_token,
      false,  // is_auto_enrollment
      device_modes,
      base::Bind(&ConsumerEnrollmentHandler::OnEnrollmentCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ConsumerEnrollmentHandler::OnEnrollmentCompleted(EnrollmentStatus status) {
  if (status.status() != EnrollmentStatus::STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to enroll the device."
               << " status=" << status.status()
               << " client_status=" << status.client_status()
               << " http_status=" << status.http_status()
               << " store_status=" << status.store_status()
               << " validation_status=" << status.validation_status();
    EndEnrollment(ConsumerManagementService::ENROLLMENT_STAGE_DM_SERVER_FAILED);
    return;
  }

  EndEnrollment(ConsumerManagementService::ENROLLMENT_STAGE_SUCCESS);
}

void ConsumerEnrollmentHandler::EndEnrollment(
    ConsumerManagementService::EnrollmentStage stage) {
  Profile* profile = enrolling_profile_;
  enrolling_profile_ = NULL;

  consumer_management_service_->SetEnrollmentStage(stage);
  if (user_manager::UserManager::Get()->IsCurrentUserOwner())
    ShowDesktopNotificationAndResetStage(stage, profile);
}

void ConsumerEnrollmentHandler::ShowDesktopNotificationAndResetStage(
    ConsumerManagementService::EnrollmentStage stage, Profile* profile) {
  base::string16 title;
  base::string16 body;
  base::string16 button_label;
  base::Closure button_click_callback;

  if (stage == ConsumerManagementService::ENROLLMENT_STAGE_SUCCESS) {
    title = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_ENROLLMENT_NOTIFICATION_TITLE);
    body = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_ENROLLMENT_NOTIFICATION_BODY);
    button_label = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_NOTIFICATION_MODIFY_SETTINGS_BUTTON);
    button_click_callback = base::Bind(
        &ConsumerEnrollmentHandler::OpenSettingsPage,
        weak_ptr_factory_.GetWeakPtr(),
        profile);
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_ENROLLMENT_FAILURE_NOTIFICATION_TITLE);
    body = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_ENROLLMENT_FAILURE_NOTIFICATION_BODY);
    button_label = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_NOTIFICATION_TRY_AGAIN_BUTTON);
    button_click_callback = base::Bind(
        &ConsumerEnrollmentHandler::TryEnrollmentAgain,
        weak_ptr_factory_.GetWeakPtr(),
        profile);
  }

  message_center::RichNotificationData optional_field;
  optional_field.buttons.push_back(message_center::ButtonInfo(button_label));
  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GURL(kEnrollmentNotificationUrl),
      title,
      body,
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_CONSUMER_MANAGEMENT_NOTIFICATION_ICON),
      blink::WebTextDirectionDefault,
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kEnrollmentNotificationId),
      base::string16(),  // display_source
      base::UTF8ToUTF16(kEnrollmentNotificationId),
      optional_field,
      new DesktopNotificationDelegate(kEnrollmentNotificationId,
                                      button_click_callback));
  notification.SetSystemPriority();
  g_browser_process->notification_ui_manager()->Add(notification, profile);

  consumer_management_service_->SetEnrollmentStage(
      ConsumerManagementService::ENROLLMENT_STAGE_NONE);
}

void ConsumerEnrollmentHandler::OpenSettingsPage(Profile* profile) const {
  const GURL url(chrome::kChromeUISettingsURL);
  chrome::NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

void ConsumerEnrollmentHandler::TryEnrollmentAgain(Profile* profile) const {
  const GURL base_url(chrome::kChromeUISettingsURL);
  const GURL url = base_url.Resolve(kConsumerManagementOverlay);

  chrome::NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

}  // namespace policy
