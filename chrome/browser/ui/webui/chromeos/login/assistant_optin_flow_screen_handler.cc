// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"

#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/arc/arc_prefs.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace {

constexpr char kJsScreenPath[] = "login.AssistantOptInFlowScreen";
constexpr char kSkipPressed[] = "skip-pressed";
constexpr char kNextPressed[] = "next-pressed";
constexpr char kFlowFinished[] = "flow-finished";

}  // namespace

AssistantOptInFlowScreenHandler::AssistantOptInFlowScreenHandler()
    : BaseScreenHandler(kScreenId), weak_factory_(this) {
  set_call_js_prefix(kJsScreenPath);
}

AssistantOptInFlowScreenHandler::~AssistantOptInFlowScreenHandler() {
  if (arc::VoiceInteractionControllerClient::Get()) {
    arc::VoiceInteractionControllerClient::Get()->RemoveObserver(this);
  }
  if (screen_) {
    screen_->OnViewDestroyed(this);
  }
}

void AssistantOptInFlowScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("assistantOptinLoading",
               IDS_VOICE_INTERACTION_VALUE_PROP_LOADING);
  builder->Add("assistantOptinLoadErrorTitle",
               IDS_VOICE_INTERACTION_VALUE_PROP_LOAD_ERROR_TITLE);
  builder->Add("assistantOptinLoadErrorMessage",
               IDS_VOICE_INTERACTION_VALUE_PROP_LOAD_ERROR_MESSAGE);
  builder->Add("assistantOptinSkipButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_SKIP_BUTTON);
  builder->Add("assistantOptinRetryButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_RETRY_BUTTON);
  builder->Add("assistantOptinOKButton", IDS_OOBE_OK_BUTTON_TEXT);
  builder->Add("assistantReadyTitle", IDS_ASSISTANT_READY_SCREEN_TITLE);
  builder->Add("assistantReadyMessage", IDS_ASSISTANT_READY_SCREEN_MESSAGE);
  builder->Add("assistantReadyButton", IDS_ASSISTANT_DONE_BUTTON);
}

void AssistantOptInFlowScreenHandler::RegisterMessages() {
  AddCallback(
      "assistant.ValuePropScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleValuePropScreenUserAction);
  AddCallback(
      "assistant.ThirdPartyScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleThirdPartyScreenUserAction);
  AddCallback("assistant.GetMoreScreen.userActed",
              &AssistantOptInFlowScreenHandler::HandleGetMoreScreenUserAction);
  AddCallback("assistant.ReadyScreen.userActed",
              &AssistantOptInFlowScreenHandler::HandleReadyScreenUserAction);
  AddCallback("assistant.ValuePropScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleValuePropScreenShown);
  AddCallback("assistant.ThirdPartyScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleThirdPartyScreenShown);
  AddCallback("assistant.GetMoreScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleGetMoreScreenShown);
  AddCallback("assistant.ReadyScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleReadyScreenShown);
  AddCallback("hotwordResult",
              &AssistantOptInFlowScreenHandler::HandleHotwordResult);
}

void AssistantOptInFlowScreenHandler::Bind(AssistantOptInFlowScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
  if (page_is_ready())
    Initialize();
}

void AssistantOptInFlowScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void AssistantOptInFlowScreenHandler::Show() {
  // Make sure enable Assistant service since we need it during the flow.
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled, false);
  prefs->SetBoolean(arc::prefs::kVoiceInteractionEnabled, true);

  if (arc::VoiceInteractionControllerClient::Get()->voice_interaction_state() ==
      ash::mojom::VoiceInteractionState::NOT_READY) {
    arc::VoiceInteractionControllerClient::Get()->AddObserver(this);
  } else {
    BindAssistantSettingsManager();
  }

  if (!page_is_ready() || !screen_) {
    show_on_init_ = true;
    return;
  }

  ShowScreen(kScreenId);
}

void AssistantOptInFlowScreenHandler::Hide() {}

void AssistantOptInFlowScreenHandler::Initialize() {
  if (!screen_ || !show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void AssistantOptInFlowScreenHandler::ShowNextScreen() {
  CallJS("showNextScreen");
}

void AssistantOptInFlowScreenHandler::OnActivityControlOptInResult(
    bool opted_in) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (opted_in) {
    RecordAssistantOptInStatus(ACTIVITY_CONTROL_ACCEPTED);
    settings_manager_->UpdateSettings(
        GetSettingsUiUpdate(consent_token_).SerializeAsString(),
        base::BindOnce(
            &AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse,
            weak_factory_.GetWeakPtr()));
  } else {
    RecordAssistantOptInStatus(ACTIVITY_CONTROL_SKIPPED);
    profile->GetPrefs()->SetBoolean(
        arc::prefs::kVoiceInteractionActivityControlAccepted, false);
    screen_->OnUserAction(kFlowFinished);
  }

  RecordActivityControlConsent(profile, ui_audit_key_, opted_in);
}

void AssistantOptInFlowScreenHandler::OnEmailOptInResult(bool opted_in) {
  if (!email_optin_needed_) {
    DCHECK(!opted_in);
    ShowNextScreen();
    return;
  }

  RecordAssistantOptInStatus(opted_in ? EMAIL_OPTED_IN : EMAIL_OPTED_OUT);
  settings_manager_->UpdateSettings(
      GetEmailOptInUpdate(opted_in).SerializeAsString(),
      base::BindOnce(&AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse,
                     weak_factory_.GetWeakPtr()));
}

void AssistantOptInFlowScreenHandler::OnStateChanged(
    ash::mojom::VoiceInteractionState state) {
  if (state != ash::mojom::VoiceInteractionState::NOT_READY) {
    BindAssistantSettingsManager();
    arc::VoiceInteractionControllerClient::Get()->RemoveObserver(this);
  }
}

void AssistantOptInFlowScreenHandler::BindAssistantSettingsManager() {
  if (settings_manager_.is_bound())
    return;

  // Set up settings mojom.
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(
          ProfileManager::GetActiveUserProfile());
  connector->BindInterface(assistant::mojom::kServiceName,
                           mojo::MakeRequest(&settings_manager_));

  SendGetSettingsRequest();
}

void AssistantOptInFlowScreenHandler::SendGetSettingsRequest() {
  assistant::SettingsUiSelector selector = GetSettingsUiSelector();
  settings_manager_->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantOptInFlowScreenHandler::OnGetSettingsResponse,
                     weak_factory_.GetWeakPtr()));
}

void AssistantOptInFlowScreenHandler::ReloadContent(const base::Value& dict) {
  CallJS("reloadContent", dict);
}

void AssistantOptInFlowScreenHandler::AddSettingZippy(const std::string& type,
                                                      const base::Value& data) {
  CallJS("addSettingZippy", type, data);
}

void AssistantOptInFlowScreenHandler::OnGetSettingsResponse(
    const std::string& settings) {
  assistant::SettingsUi settings_ui;
  settings_ui.ParseFromString(settings);

  DCHECK(settings_ui.has_consent_flow_ui());

  RecordAssistantOptInStatus(FLOW_STARTED);
  auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
  auto activity_control_ui = consent_ui.activity_control_ui();
  auto third_party_disclosure_ui = consent_ui.third_party_disclosure_ui();

  consent_token_ = activity_control_ui.consent_token();
  ui_audit_key_ = activity_control_ui.ui_audit_key();

  // Process activity control data.
  if (!activity_control_ui.setting_zippy().size()) {
    // No need to consent. Move to the next screen.
    activity_control_needed_ = false;
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                      true);
    ShowNextScreen();
  } else {
    AddSettingZippy("settings",
                    CreateZippyData(activity_control_ui.setting_zippy()));
  }

  // Process third party disclosure data.
  AddSettingZippy("disclosure", CreateDisclosureData(
                                    third_party_disclosure_ui.disclosures()));

  // Process get more data.
  email_optin_needed_ = settings_ui.has_email_opt_in_ui() &&
                        settings_ui.email_opt_in_ui().has_title();
  AddSettingZippy("get-more", CreateGetMoreData(email_optin_needed_,
                                                settings_ui.email_opt_in_ui()));

  // Pass string constants dictionary.
  ReloadContent(GetSettingsUiStrings(settings_ui, activity_control_needed_));
}

void AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse(
    const std::string& result) {
  assistant::SettingsUiUpdateResult ui_result;
  ui_result.ParseFromString(result);

  if (ui_result.has_consent_flow_update_result()) {
    if (ui_result.consent_flow_update_result().update_status() !=
        assistant::ConsentFlowUiUpdateResult::SUCCESS) {
      // TODO(updowndta): Handle consent update failure.
      LOG(ERROR) << "Consent udpate error.";
    } else if (activity_control_needed_) {
      activity_control_needed_ = false;
      PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
      prefs->SetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted,
                        true);
    }
  }

  if (ui_result.has_email_opt_in_update_result()) {
    if (ui_result.email_opt_in_update_result().update_status() !=
        assistant::EmailOptInUpdateResult::SUCCESS) {
      // TODO(updowndta): Handle email optin update failure.
      LOG(ERROR) << "Email OptIn udpate error.";
    }
    // Update hotword will cause Assistant restart. In order to make sure email
    // optin request is successfully sent to server, update the hotword after
    // email optin result has been received.
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled,
                      enable_hotword_);
  }

  ShowNextScreen();
}

void AssistantOptInFlowScreenHandler::HandleHotwordResult(bool enable_hotword) {
  enable_hotword_ = enable_hotword;

  if (!email_optin_needed_) {
    // No need to send email optin result. Safe to update hotword pref and
    // restart Assistant here.
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled,
                      enable_hotword);
  }
}

void AssistantOptInFlowScreenHandler::HandleValuePropScreenUserAction(
    const std::string& action) {
  if (action == kSkipPressed) {
    OnActivityControlOptInResult(false);
  } else if (action == kNextPressed) {
    OnActivityControlOptInResult(true);
  }
}

void AssistantOptInFlowScreenHandler::HandleThirdPartyScreenUserAction(
    const std::string& action) {
  if (action == kNextPressed) {
    RecordAssistantOptInStatus(THIRD_PARTY_CONTINUED);
    ShowNextScreen();
  }
}

void AssistantOptInFlowScreenHandler::HandleGetMoreScreenUserAction(
    const bool screen_context,
    const bool email_opted_in) {
  RecordAssistantOptInStatus(GET_MORE_CONTINUED);
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(arc::prefs::kVoiceInteractionContextEnabled,
                    screen_context);
  OnEmailOptInResult(email_opted_in);
}

void AssistantOptInFlowScreenHandler::HandleReadyScreenUserAction(
    const std::string& action) {
  if (action == kNextPressed) {
    RecordAssistantOptInStatus(READY_SCREEN_CONTINUED);
    screen_->OnUserAction(kFlowFinished);
  }
}

void AssistantOptInFlowScreenHandler::HandleValuePropScreenShown() {
  RecordAssistantOptInStatus(ACTIVITY_CONTROL_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleThirdPartyScreenShown() {
  RecordAssistantOptInStatus(THIRD_PARTY_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleGetMoreScreenShown() {
  RecordAssistantOptInStatus(GET_MORE_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleReadyScreenShown() {
  RecordAssistantOptInStatus(READY_SCREEN_SHOWN);
}

}  // namespace chromeos
