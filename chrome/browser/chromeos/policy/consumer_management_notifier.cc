// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_notifier.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

namespace {

// Desktop notification constants.
const char kEnrollmentNotificationId[] = "consumer_management.enroll";
const char kEnrollmentNotificationUrl[] = "chrome://consumer-management/enroll";

// The path to the consumer management enrollment/unenrollment confirmation
// overlay, relative to the settings page URL.
const char kConsumerManagementOverlay[] = "consumer-management-overlay";

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

ConsumerManagementNotifier::ConsumerManagementNotifier(
    Profile* profile,
    ConsumerManagementService* consumer_management_service)
    : profile_(profile),
      consumer_management_service_(consumer_management_service),
      weak_ptr_factory_(this) {
  consumer_management_service_->AddObserver(this);
  OnConsumerManagementStatusChanged();
}

ConsumerManagementNotifier::~ConsumerManagementNotifier() {
}

void ConsumerManagementNotifier::Shutdown() {
  consumer_management_service_->RemoveObserver(this);
}

void ConsumerManagementNotifier::OnConsumerManagementStatusChanged() {
  if (consumer_management_service_->HasPendingEnrollmentNotification()) {
    ConsumerManagementService::EnrollmentStage stage =
        consumer_management_service_->GetEnrollmentStage();
    // Reset the enrollment stage so that we won't show the same notification,
    // again.
    consumer_management_service_->SetEnrollmentStage(
        ConsumerManagementService::ENROLLMENT_STAGE_NONE);
    ShowDesktopNotification(stage);
  }
}

void ConsumerManagementNotifier::ShowDesktopNotification(
    ConsumerManagementService::EnrollmentStage stage) {
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
        &ConsumerManagementNotifier::OpenSettingsPage,
        weak_ptr_factory_.GetWeakPtr());
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_ENROLLMENT_FAILURE_NOTIFICATION_TITLE);
    body = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_ENROLLMENT_FAILURE_NOTIFICATION_BODY);
    button_label = l10n_util::GetStringUTF16(
        IDS_CONSUMER_MANAGEMENT_NOTIFICATION_TRY_AGAIN_BUTTON);
    button_click_callback = base::Bind(
        &ConsumerManagementNotifier::TryEnrollmentAgain,
        weak_ptr_factory_.GetWeakPtr());
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
  g_browser_process->notification_ui_manager()->Add(notification, profile_);
}

void ConsumerManagementNotifier::OpenSettingsPage() const {
  const GURL url(chrome::kChromeUISettingsURL);
  chrome::NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

void ConsumerManagementNotifier::TryEnrollmentAgain() const {
  const GURL base_url(chrome::kChromeUISettingsURL);
  const GURL url = base_url.Resolve(kConsumerManagementOverlay);

  chrome::NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

}  // namespace policy
