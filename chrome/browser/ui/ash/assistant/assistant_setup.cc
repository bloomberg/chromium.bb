// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_setup.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {

constexpr char kAssistantDisplaySource[] = "Assistant";
constexpr char kAssistantSubPage[] = "googleAssistant";
constexpr char kHotwordNotificationId[] = "assistant/hotword";
constexpr char kNotifierAssistant[] = "assistant";
constexpr int kAssistantIconSize = 24;

// Delegate for assistant hotword notification.
class AssistantHotwordNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit AssistantHotwordNotificationDelegate(Profile* profile)
      : profile_(profile) {}

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    if (!by_user)
      return;

    HandleHotwordEnableNotificationResult(false /* enable */);
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    HandleHotwordEnableNotificationResult(true /* enable */);
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, kHotwordNotificationId);
  }

 private:
  ~AssistantHotwordNotificationDelegate() override = default;

  void HandleHotwordEnableNotificationResult(bool enable) {
    if (enable) {
      chrome::ShowSettingsSubPageForProfile(
          ProfileManager::GetActiveUserProfile(), kAssistantSubPage);
    }
    UMA_HISTOGRAM_BOOLEAN("Assistant.HotwordEnableNotification", enable);
  }

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AssistantHotwordNotificationDelegate);
};

}  // namespace

AssistantSetup::AssistantSetup(service_manager::Connector* connector)
    : binding_(this) {
  // Bind to the Assistant controller in ash.
  ash::mojom::AssistantControllerPtr assistant_controller;
  connector->BindInterface(ash::mojom::kServiceName, &assistant_controller);
  ash::mojom::AssistantSetupPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_controller->SetAssistantSetup(std::move(ptr));

  arc::VoiceInteractionControllerClient::Get()->AddObserver(this);
}

AssistantSetup::~AssistantSetup() {
  arc::VoiceInteractionControllerClient::Get()->RemoveObserver(this);
}

void AssistantSetup::StartAssistantOptInFlow(
    StartAssistantOptInFlowCallback callback) {
  if (chromeos::AssistantOptInDialog::IsActive())
    return;

  chromeos::AssistantOptInDialog::Show(std::move(callback));
}

void AssistantSetup::OnStateChanged(ash::mojom::VoiceInteractionState state) {
  if (state != ash::mojom::VoiceInteractionState::RUNNING)
    return;

  // If the optin flow is active, no need to show the notification since it is
  // included in the flow.
  if (chromeos::AssistantOptInDialog::IsActive())
    return;

  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->FindPreference(arc::prefs::kVoiceInteractionHotwordEnabled)
           ->IsDefaultValue()) {
    return;
  }
  // Avoid the notification from keep showing up.
  prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled, false);

  const base::string16 title =
      l10n_util::GetStringUTF16(IDS_ASSISTANT_HOTWORD_NOTIFICATION_TITLE);
  const base::string16 display_source =
      base::UTF8ToUTF16(kAssistantDisplaySource);

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kHotwordNotificationId, title,
      base::string16(), gfx::Image(), display_source, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kNotifierAssistant),
      {}, base::MakeRefCounted<AssistantHotwordNotificationDelegate>(profile));

  gfx::Image image(CreateVectorIcon(ash::kNotificationAssistantIcon,
                                    kAssistantIconSize, gfx::kGoogleBlue700));
  notification.set_small_image(image);

  NotificationDisplayService::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification);
}
