// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_support_host.h"

#include <string>

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "base/i18n/timezone.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/screen.h"

namespace {
constexpr char kAction[] = "action";
constexpr char kArcManaged[] = "arcManaged";
constexpr char kData[] = "data";
constexpr char kDeviceId[] = "deviceId";
constexpr char kPage[] = "page";
constexpr char kStatus[] = "status";
constexpr char kActionInitialize[] = "initialize";
constexpr char kActionSetMetricsMode[] = "setMetricsMode";
constexpr char kActionBackupAndRestoreMode[] = "setBackupAndRestoreMode";
constexpr char kActionLocationServiceMode[] = "setLocationServiceMode";
constexpr char kActionSetWindowBounds[] = "setWindowBounds";
constexpr char kActionCloseWindow[] = "closeWindow";
constexpr char kActionShowPage[] = "showPage";

// The preference update should have those two fields.
constexpr char kEnabled[] = "enabled";
constexpr char kManaged[] = "managed";

// The JSON data sent from the extension should have at least "event" field.
// Each event data is defined below.
// The key of the event type.
constexpr char kEvent[] = "event";

// "onWindowClosed" is fired when the extension window is closed.
// No data will be provided.
constexpr char kEventOnWindowClosed[] = "onWindowClosed";

// "onAuthSucceeded" is fired when successfully done to LSO authorization in
// extension.
// The auth token is passed via "code" field.
constexpr char kEventOnAuthSuccedded[] = "onAuthSucceeded";
constexpr char kCode[] = "code";

// "onAgree" is fired when a user clicks "Agree" button.
// The message should have the following three fields:
// - isMetricsEnabled
// - isBackupRestoreEnabled
// - isLocationServiceEnabled
constexpr char kEventOnAgreed[] = "onAgreed";
constexpr char kIsMetricsEnabled[] = "isMetricsEnabled";
constexpr char kIsBackupRestoreEnabled[] = "isBackupRestoreEnabled";
constexpr char kIsLocationServiceEnabled[] = "isLocationServiceEnabled";

// "onSendFeedbackClicked" is fired when a user clicks "Send Feedback" button.
constexpr char kEventOnSendFeedbackClicked[] = "onSendFeedbackClicked";

}  // namespace

// static
const char ArcSupportHost::kHostAppId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";

// static
const char ArcSupportHost::kStorageId[] = "arc_support";

ArcSupportHost::ArcSupportHost() {
  // TODO(hidehiko): Get rid of dependency to ArcAuthService.
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);

  if (!arc_auth_service->IsAllowed())
    return;

  // TODO(hidehiko): This if statement is only for testing. Clean up this.
  if (g_browser_process->local_state()) {
    pref_local_change_registrar_.Init(g_browser_process->local_state());
    pref_local_change_registrar_.Add(
        metrics::prefs::kMetricsReportingEnabled,
        base::Bind(&ArcSupportHost::OnMetricsPreferenceChanged,
                   base::Unretained(this)));
  }

  pref_change_registrar_.Init(arc_auth_service->profile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcBackupRestoreEnabled,
      base::Bind(&ArcSupportHost::OnBackupAndRestorePreferenceChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kArcLocationServiceEnabled,
      base::Bind(&ArcSupportHost::OnLocationServicePreferenceChanged,
                 base::Unretained(this)));
}

ArcSupportHost::~ArcSupportHost() {
  if (message_host_)
    DisconnectMessageHost();
}

void ArcSupportHost::Close() {
  if (!message_host_) {
    VLOG(2) << "ArcSupportHost::Close() is called "
            << "but message_host_ is not available.";
    return;
  }

  base::DictionaryValue message;
  message.SetString(kAction, kActionCloseWindow);
  message_host_->SendMessage(message);

  // Disconnect immediately, so that onWindowClosed event will not be
  // delivered to here.
  DisconnectMessageHost();
}

void ArcSupportHost::ShowPage(arc::ArcAuthService::UIPage page,
                              const base::string16& status) {
  if (!message_host_) {
    VLOG(2) << "ArcSupportHost::ShowPage() is called "
            << "but message_host_ is not available.";
    return;
  }

  base::DictionaryValue message;
  message.SetString(kAction, kActionShowPage);
  message.SetInteger(kPage, static_cast<int>(page));
  message.SetString(kStatus, status);
  message_host_->SendMessage(message);
}

void ArcSupportHost::SetMessageHost(arc::ArcSupportMessageHost* message_host) {
  if (message_host_ == message_host)
    return;

  if (message_host_)
    DisconnectMessageHost();
  message_host_ = message_host;
  message_host_->SetObserver(this);
  display::Screen::GetScreen()->AddObserver(this);

  if (!Initialize()) {
    Close();
    return;
  }

  SendMetricsMode();
  SendBackupAndRestoreMode();
  SendLocationServicesMode();

  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);
  ShowPage(arc_auth_service->ui_page(), arc_auth_service->ui_page_status());
}

void ArcSupportHost::UnsetMessageHost(
    arc::ArcSupportMessageHost* message_host) {
  if (message_host_ != message_host)
    return;
  DisconnectMessageHost();
}

void ArcSupportHost::DisconnectMessageHost() {
  DCHECK(message_host_);
  display::Screen::GetScreen()->RemoveObserver(this);
  message_host_->SetObserver(nullptr);
  message_host_ = nullptr;
}

bool ArcSupportHost::Initialize() {
  DCHECK(message_host_);
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  if (!arc_auth_service->IsAllowed())
    return false;

  std::unique_ptr<base::DictionaryValue> loadtime_data(
      new base::DictionaryValue());
  base::string16 device_name = ash::GetChromeOSDeviceName();
  loadtime_data->SetString(
      "greetingHeader",
      l10n_util::GetStringFUTF16(IDS_ARC_OPT_IN_DIALOG_HEADER, device_name));
  loadtime_data->SetString("greetingDescription",
                           l10n_util::GetStringFUTF16(
                               IDS_ARC_OPT_IN_DIALOG_DESCRIPTION, device_name));
  loadtime_data->SetString(
      "buttonAgree",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE));
  loadtime_data->SetString(
      "buttonCancel",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_CANCEL));
  loadtime_data->SetString(
      "buttonSendFeedback",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_SEND_FEEDBACK));
  loadtime_data->SetString(
      "buttonRetry",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_RETRY));
  loadtime_data->SetString(
      "progressLsoLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_LSO));
  loadtime_data->SetString(
      "progressAndroidLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_ANDROID));
  loadtime_data->SetString(
      "authorizationFailed",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_AUTHORIZATION_FAILED));
  loadtime_data->SetString(
      "termsOfService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_TERMS_OF_SERVICE));
  loadtime_data->SetString(
      "textMetricsEnabled",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED));
  loadtime_data->SetString(
      "textMetricsDisabled",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED));
  loadtime_data->SetString(
      "textMetricsManagedEnabled",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED));
  loadtime_data->SetString("textMetricsManagedDisabled",
                           l10n_util::GetStringUTF16(
                               IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED));
  loadtime_data->SetString(
      "textBackupRestore",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE));
  loadtime_data->SetString(
      "textLocationService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LOCATION_SETTING));
  loadtime_data->SetString(
      "serverError",
      l10n_util::GetStringUTF16(IDS_ARC_SERVER_COMMUNICATION_ERROR));
  loadtime_data->SetString(
      "controlledByPolicy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_POLICY));
  loadtime_data->SetString(
      "learnMoreStatistics",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS));
  loadtime_data->SetString(
      "learnMoreBackupAndRestore",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE));
  loadtime_data->SetString(
      "learnMoreLocationServices",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES));
  loadtime_data->SetString(
      "overlayClose",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_CLOSE));
  loadtime_data->SetString(
      "privacyPolicyLink",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  const std::string& country_code = base::CountryCodeForCurrentTimezone();
  loadtime_data->SetString("countryCode", country_code);
  loadtime_data->SetBoolean(kArcManaged, arc_auth_service->IsArcManaged());

  webui::SetLoadTimeDataDefaults(app_locale, loadtime_data.get());
  DCHECK(arc_auth_service);
  const std::string device_id = user_manager::known_user::GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(arc_auth_service->profile()));
  DCHECK(!device_id.empty());
  loadtime_data->SetBoolean(
      "isOwnerProfile",
      chromeos::ProfileHelper::IsOwnerProfile(arc_auth_service->profile()));

  base::DictionaryValue message;
  message.SetString(kAction, kActionInitialize);
  message.Set(kData, std::move(loadtime_data));
  message.SetString(kDeviceId, device_id);
  message_host_->SendMessage(message);
  return true;
}

void ArcSupportHost::OnDisplayAdded(const display::Display& new_display) {}

void ArcSupportHost::OnDisplayRemoved(const display::Display& old_display) {}

void ArcSupportHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  if (!message_host_)
    return;

  base::DictionaryValue message;
  message.SetString(kAction, kActionSetWindowBounds);
  message_host_->SendMessage(message);
}

void ArcSupportHost::OnMetricsPreferenceChanged() {
  SendMetricsMode();
}

void ArcSupportHost::OnBackupAndRestorePreferenceChanged() {
  SendBackupAndRestoreMode();
}

void ArcSupportHost::OnLocationServicePreferenceChanged() {
  SendLocationServicesMode();
}

void ArcSupportHost::SendMetricsMode() {
  SendPreferenceUpdate(
      kActionSetMetricsMode,
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(),
      IsMetricsReportingPolicyManaged());
}

void ArcSupportHost::SendBackupAndRestoreMode() {
  SendOptionMode(kActionBackupAndRestoreMode, prefs::kArcBackupRestoreEnabled);
}

void ArcSupportHost::SendLocationServicesMode() {
  SendOptionMode(kActionLocationServiceMode, prefs::kArcLocationServiceEnabled);
}

void ArcSupportHost::SendOptionMode(const std::string& action_name,
                                    const std::string& pref_name) {
  const arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);
  SendPreferenceUpdate(
      action_name,
      arc_auth_service->profile()->GetPrefs()->GetBoolean(pref_name),
      arc_auth_service->profile()->GetPrefs()->IsManagedPreference(pref_name));
}

void ArcSupportHost::SendPreferenceUpdate(const std::string& action_name,
                                          bool is_enabled,
                                          bool is_managed) {
  if (!message_host_)
    return;

  base::DictionaryValue message;
  message.SetString(kAction, action_name);
  message.SetBoolean(kEnabled, is_enabled);
  message.SetBoolean(kManaged, is_managed);
  message_host_->SendMessage(message);
}

void ArcSupportHost::EnableMetrics(bool is_enabled) {
  ChangeMetricsReportingState(is_enabled);
}

void ArcSupportHost::EnableBackupRestore(bool is_enabled) {
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service && arc_auth_service->IsAllowed());
  PrefService* pref_service = arc_auth_service->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kArcBackupRestoreEnabled, is_enabled);
}

void ArcSupportHost::EnableLocationService(bool is_enabled) {
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service && arc_auth_service->IsAllowed());
  PrefService* pref_service = arc_auth_service->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kArcLocationServiceEnabled, is_enabled);
}

void ArcSupportHost::OnMessage(const base::DictionaryValue& message) {
  std::string event;
  if (!message.GetString(kEvent, &event)) {
    NOTREACHED();
    return;
  }

  // TODO(hidehiko): Replace by Observer.
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);

  if (event == kEventOnWindowClosed) {
    arc_auth_service->CancelAuthCode();
  } else if (event == kEventOnAuthSuccedded) {
    std::string code;
    if (message.GetString(kCode, &code)) {
      arc_auth_service->SetAuthCodeAndStartArc(code);
    } else {
      NOTREACHED();
    }
  } else if (event == kEventOnAgreed) {
    bool is_metrics_enabled;
    bool is_backup_restore_enabled;
    bool is_location_service_enabled;
    if (message.GetBoolean(kIsMetricsEnabled, &is_metrics_enabled) &&
        message.GetBoolean(kIsBackupRestoreEnabled,
                           &is_backup_restore_enabled) &&
        message.GetBoolean(kIsLocationServiceEnabled,
                           &is_location_service_enabled)) {
      EnableMetrics(is_metrics_enabled);
      EnableBackupRestore(is_backup_restore_enabled);
      EnableLocationService(is_location_service_enabled);
      arc_auth_service->StartLso();
    } else {
      NOTREACHED();
    }
  } else if (event == kEventOnSendFeedbackClicked) {
    chrome::OpenFeedbackDialog(nullptr);
  } else {
    LOG(ERROR) << "Unknown message: " << event;
    NOTREACHED();
  }
}
