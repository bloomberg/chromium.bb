// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_settings_controller_chromeos.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/i18n/string_compare.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/arc_application_notifier_controller_chromeos.h"
#include "chrome/browser/notifications/extension_notifier_controller.h"
#include "chrome/browser/notifications/web_page_notifier_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/image/image.h"

using message_center::Notifier;
using message_center::NotifierId;

namespace {

class NotifierComparator {
 public:
  explicit NotifierComparator(icu::Collator* collator) : collator_(collator) {}

  bool operator()(const std::unique_ptr<Notifier>& n1,
                  const std::unique_ptr<Notifier>& n2) {
    if (n1->notifier_id.type != n2->notifier_id.type)
      return n1->notifier_id.type < n2->notifier_id.type;

    if (collator_) {
      return base::i18n::CompareString16WithCollator(*collator_, n1->name,
                                                     n2->name) == UCOL_LESS;
    }
    return n1->name < n2->name;
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

MessageCenterSettingsControllerChromeOs::
    MessageCenterSettingsControllerChromeOs() {
  sources_.insert(std::make_pair(NotifierId::APPLICATION,
                                 std::unique_ptr<NotifierController>(
                                     new ExtensionNotifierController(this))));

  sources_.insert(std::make_pair(NotifierId::WEB_PAGE,
                                 std::unique_ptr<NotifierController>(
                                     new WebPageNotifierController(this))));

  sources_.insert(std::make_pair(
      NotifierId::ARC_APPLICATION,
      std::unique_ptr<NotifierController>(
          new arc::ArcApplicationNotifierControllerChromeOS(this))));
}

MessageCenterSettingsControllerChromeOs::
    ~MessageCenterSettingsControllerChromeOs() {}

void MessageCenterSettingsControllerChromeOs::AddObserver(
    message_center::NotifierSettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void MessageCenterSettingsControllerChromeOs::RemoveObserver(
    message_center::NotifierSettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MessageCenterSettingsControllerChromeOs::GetNotifierList(
    std::vector<std::unique_ptr<Notifier>>* notifiers) {
  DCHECK(notifiers);
  for (auto& source : sources_) {
    auto source_notifiers = source.second->GetNotifierList(GetProfile());
    for (auto& notifier : source_notifiers) {
      notifiers->push_back(std::move(notifier));
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  NotifierComparator comparator(U_SUCCESS(error) ? collator.get() : nullptr);

  std::sort(notifiers->begin(), notifiers->end(), comparator);
}

void MessageCenterSettingsControllerChromeOs::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  sources_[notifier_id.type]->SetNotifierEnabled(GetProfile(), notifier_id,
                                                 enabled);
}

void MessageCenterSettingsControllerChromeOs::OnNotifierSettingsClosing() {
  for (auto& source : sources_) {
    source.second->OnNotifierSettingsClosing();
  }
}

bool MessageCenterSettingsControllerChromeOs::NotifierHasAdvancedSettings(
    const NotifierId& notifier_id) const {
  return sources_.find(notifier_id.type)
      ->second->HasAdvancedSettings(GetProfile(), notifier_id);
}

void MessageCenterSettingsControllerChromeOs::
    OnNotifierAdvancedSettingsRequested(const NotifierId& notifier_id,
                                        const std::string* notification_id) {
  return sources_[notifier_id.type]->OnNotifierAdvancedSettingsRequested(
      GetProfile(), notifier_id, notification_id);
}

void MessageCenterSettingsControllerChromeOs::OnIconImageUpdated(
    const message_center::NotifierId& id,
    const gfx::Image& image) {
  for (message_center::NotifierSettingsObserver& observer : observers_)
    observer.UpdateIconImage(id, image);
}

Profile* MessageCenterSettingsControllerChromeOs::GetProfile() const {
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
}

void MessageCenterSettingsControllerChromeOs::OnNotifierEnabledChanged(
    const message_center::NotifierId& id,
    bool enabled) {
  for (message_center::NotifierSettingsObserver& observer : observers_)
    observer.NotifierEnabledChanged(id, enabled);
}
