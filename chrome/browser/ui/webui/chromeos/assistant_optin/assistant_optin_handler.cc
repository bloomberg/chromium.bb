// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/arc/arc_prefs.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace {

constexpr char kJsScreenPath[] = "assistantOptInFlow";

}  // namespace

AssistantOptInHandler::AssistantOptInHandler(
    JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container), weak_factory_(this) {
  DCHECK(js_calls_container);
  set_call_js_prefix(kJsScreenPath);
}

AssistantOptInHandler::~AssistantOptInHandler() {
  if (arc::VoiceInteractionControllerClient::Get()) {
    arc::VoiceInteractionControllerClient::Get()->RemoveObserver(this);
  }
}

void AssistantOptInHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void AssistantOptInHandler::RegisterMessages() {
  AddPrefixedCallback("initialized", &AssistantOptInHandler::HandleInitialized);
  AddPrefixedCallback("hotwordResult",
                      &AssistantOptInHandler::HandleHotwordResult);
  AddPrefixedCallback("flowFinished",
                      &AssistantOptInHandler::HandleFlowFinished);
}

void AssistantOptInHandler::Initialize() {
  if (arc::VoiceInteractionControllerClient::Get()->voice_interaction_state() ==
      ash::mojom::VoiceInteractionState::NOT_READY) {
    arc::VoiceInteractionControllerClient::Get()->AddObserver(this);
  } else {
    BindAssistantSettingsManager();
  }
}

void AssistantOptInHandler::ShowNextScreen() {
  CallJSOrDefer("showNextScreen");
}

void AssistantOptInHandler::OnActivityControlOptInResult(bool opted_in) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (opted_in) {
    RecordAssistantOptInStatus(ACTIVITY_CONTROL_ACCEPTED);
    settings_manager_->UpdateSettings(
        GetSettingsUiUpdate(consent_token_).SerializeAsString(),
        base::BindOnce(&AssistantOptInHandler::OnUpdateSettingsResponse,
                       weak_factory_.GetWeakPtr()));
  } else {
    RecordAssistantOptInStatus(ACTIVITY_CONTROL_SKIPPED);
    profile->GetPrefs()->SetBoolean(
        arc::prefs::kVoiceInteractionActivityControlAccepted, false);
    CallJSOrDefer("closeDialog");
  }

  RecordActivityControlConsent(profile, ui_audit_key_, opted_in);
}

void AssistantOptInHandler::OnEmailOptInResult(bool opted_in) {
  if (!email_optin_needed_) {
    DCHECK(!opted_in);
    ShowNextScreen();
    return;
  }

  RecordAssistantOptInStatus(opted_in ? EMAIL_OPTED_IN : EMAIL_OPTED_OUT);
  settings_manager_->UpdateSettings(
      GetEmailOptInUpdate(opted_in).SerializeAsString(),
      base::BindOnce(&AssistantOptInHandler::OnUpdateSettingsResponse,
                     weak_factory_.GetWeakPtr()));
}

void AssistantOptInHandler::OnStateChanged(
    ash::mojom::VoiceInteractionState state) {
  if (state != ash::mojom::VoiceInteractionState::NOT_READY) {
    BindAssistantSettingsManager();
    arc::VoiceInteractionControllerClient::Get()->RemoveObserver(this);
  }
}

void AssistantOptInHandler::BindAssistantSettingsManager() {
  if (settings_manager_.is_bound())
    return;

  // Set up settings mojom.
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(Profile::FromWebUI(web_ui()));
  connector->BindInterface(assistant::mojom::kServiceName,
                           mojo::MakeRequest(&settings_manager_));

  SendGetSettingsRequest();
}

void AssistantOptInHandler::SendGetSettingsRequest() {
  assistant::SettingsUiSelector selector = GetSettingsUiSelector();
  settings_manager_->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantOptInHandler::OnGetSettingsResponse,
                     weak_factory_.GetWeakPtr()));
}

void AssistantOptInHandler::ReloadContent(const base::Value& dict) {
  CallJSOrDefer("reloadContent", dict);
}

void AssistantOptInHandler::AddSettingZippy(const std::string& type,
                                            const base::Value& data) {
  CallJSOrDefer("addSettingZippy", type, data);
}

void AssistantOptInHandler::OnGetSettingsResponse(const std::string& settings) {
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
  bool skip_activity_control = !activity_control_ui.setting_zippy().size();
  if (skip_activity_control) {
    // No need to consent. Move to the next screen.
    activity_control_needed_ = false;
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetBoolean(
        arc::prefs::kVoiceInteractionActivityControlAccepted,
        (settings_ui.consent_flow_ui().consent_status() ==
             assistant::ConsentFlowUi_ConsentStatus_ALREADY_CONSENTED ||
         settings_ui.consent_flow_ui().consent_status() ==
             assistant::ConsentFlowUi_ConsentStatus_ASK_FOR_CONSENT));
    // Skip activity control and users will be in opted out mode.
    ShowNextScreen();
  } else {
    AddSettingZippy("settings",
                    CreateZippyData(activity_control_ui.setting_zippy()));
  }

  // Process third party disclosure data.
  bool skip_third_party_disclosure =
      skip_activity_control && !third_party_disclosure_ui.disclosures().size();
  if (third_party_disclosure_ui.disclosures().size()) {
    AddSettingZippy("disclosure", CreateDisclosureData(
                                      third_party_disclosure_ui.disclosures()));
  } else if (skip_third_party_disclosure) {
    ShowNextScreen();
  } else {
    // TODO(llin): Show an error message and log it properly.
    LOG(ERROR) << "Missing third Party disclosure data.";
    return;
  }

  // Process get more data.
  email_optin_needed_ = settings_ui.has_email_opt_in_ui() &&
                        settings_ui.email_opt_in_ui().has_title();
  auto get_more_data =
      CreateGetMoreData(email_optin_needed_, settings_ui.email_opt_in_ui());

  bool skip_get_more =
      skip_third_party_disclosure && !get_more_data.GetList().size();
  if (get_more_data.GetList().size()) {
    AddSettingZippy("get-more", get_more_data);
  } else if (skip_get_more) {
    ShowNextScreen();
  } else {
    // TODO(llin): Show an error message and log it properly.
    LOG(ERROR) << "Missing get more data.";
    return;
  }

  // Pass string constants dictionary.
  ReloadContent(GetSettingsUiStrings(settings_ui, activity_control_needed_));
}

void AssistantOptInHandler::OnUpdateSettingsResponse(
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
      PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
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
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled,
                      enable_hotword_);
  }

  ShowNextScreen();
}

void AssistantOptInHandler::HandleInitialized() {
  ExecuteDeferredJSCalls();
}

void AssistantOptInHandler::HandleHotwordResult(bool enable_hotword) {
  enable_hotword_ = enable_hotword;

  if (!email_optin_needed_) {
    // No need to send email optin result. Safe to update hotword pref and
    // restart Assistant here.
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetBoolean(arc::prefs::kVoiceInteractionHotwordEnabled,
                      enable_hotword);
  }
}

void AssistantOptInHandler::HandleFlowFinished() {
  CallJSOrDefer("closeDialog");
}

}  // namespace chromeos
