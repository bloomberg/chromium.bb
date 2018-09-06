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
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using chromeos::assistant::ConsentFlowUi;

namespace {

constexpr char kAssistantDisplaySource[] = "Assistant";
constexpr char kAssistantSubPage[] = "googleAssistant";
constexpr char kHotwordNotificationId[] = "assistant/hotword";
constexpr char kNotifierAssistant[] = "assistant";

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
    : connector_(connector), binding_(this) {
  // Bind to the Assistant controller in ash.
  ash::mojom::AssistantControllerPtr assistant_controller;
  connector_->BindInterface(ash::mojom::kServiceName, &assistant_controller);
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
  if (state == ash::mojom::VoiceInteractionState::NOT_READY)
    return;

  // Sync activity control state when assistant service started.
  if (!settings_manager_)
    SyncActivityControlState();

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
  // TODO(xiaohuic): need better ways to decide when to show the notification.
  // Avoid the notification from keep showing up.
  prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled, false);

  const base::string16 title =
      l10n_util::GetStringUTF16(IDS_ASSISTANT_HOTWORD_NOTIFICATION_TITLE);
  const base::string16 display_source =
      base::UTF8ToUTF16(kAssistantDisplaySource);

  auto notification = message_center::Notification::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kHotwordNotificationId, title,
      base::string16(), display_source, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kNotifierAssistant),
      {}, base::MakeRefCounted<AssistantHotwordNotificationDelegate>(profile),
      ash::kNotificationAssistantIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  NotificationDisplayService::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification);
}

void AssistantSetup::SyncActivityControlState() {
  // Set up settings mojom.
  connector_->BindInterface(chromeos::assistant::mojom::kServiceName,
                            mojo::MakeRequest(&settings_manager_));

  chromeos::assistant::SettingsUiSelector selector;
  chromeos::assistant::ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(
      chromeos::assistant::ActivityControlSettingsUiSelector::
          ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  settings_manager_->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantSetup::OnGetSettingsResponse,
                     base::Unretained(this)));
}

void AssistantSetup::OnGetSettingsResponse(const std::string& settings) {
  chromeos::assistant::SettingsUi settings_ui;
  settings_ui.ParseFromString(settings);
  if (!settings_ui.has_consent_flow_ui()) {
    LOG(ERROR) << "Failed to get activity control status.";
    return;
  }

  auto consent_status = settings_ui.consent_flow_ui().consent_status();
  auto& consent_ui = settings_ui.consent_flow_ui().consent_ui();
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* prefs = profile->GetPrefs();
  switch (consent_status) {
    case ConsentFlowUi::ASK_FOR_CONSENT:
      if (consent_ui.has_activity_control_ui() &&
          consent_ui.activity_control_ui().setting_zippy().size()) {
        prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                          false);
      } else {
        prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                          true);
      }
      break;
    case ConsentFlowUi::ERROR_ACCOUNT:
      // Show the opted out mode UI for unsupported Account as they are in opted
      // out mode.
      // TODO(llin): we should show a error account message in Opted out UI or
      // in the onboarding flow.
      prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                        false);
      break;
    case ConsentFlowUi::ALREADY_CONSENTED:
      prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                        true);
      break;
    case ConsentFlowUi::UNSPECIFIED:
    case ConsentFlowUi::ERROR:
      prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                        false);
      LOG(ERROR) << "Invalid activity control consent status.";
  }
}
