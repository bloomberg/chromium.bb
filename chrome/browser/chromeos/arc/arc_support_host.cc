// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_support_host.h"

#include "ash/system/chromeos/devicetype_utils.h"
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
#include "chrome/grit/generated_resources.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace {
const char kAction[] = "action";
const char kCode[] = "code";
const char kCanEnable[] = "canEnable";
const char kStatus[] = "status";
const char kData[] = "data";
const char kOn[] = "on";
const char kPage[] = "page";
const char kText[] = "text";
const char kActionInitialize[] = "initialize";
const char kActionSetMetricsMode[] = "setMetricsMode";
const char kActionStartLso[] = "startLso";
const char kActionCancelAuthCode[] = "cancelAuthCode";
const char kActionSetAuthCode[] = "setAuthCode";
const char kActionEnableMetrics[] = "enableMetrics";
const char kActionCloseUI[] = "closeUI";
const char kActionShowPage[] = "showPage";
}  // namespace

// static
const char ArcSupportHost::kHostName[] = "com.google.arc_support";

// static
const char ArcSupportHost::kHostAppId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";

// static
const char ArcSupportHost::kStorageId[] = "arc_support";

// static
const char* const ArcSupportHost::kHostOrigin[] = {
    "chrome-extension://cnbgggchhmkkdmeppjobngjoejnihlei/"};

// static
std::unique_ptr<extensions::NativeMessageHost> ArcSupportHost::Create() {
  return std::unique_ptr<NativeMessageHost>(new ArcSupportHost());
}

ArcSupportHost::ArcSupportHost() {
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);
  arc_auth_service->AddObserver(this);

  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      metrics::prefs::kMetricsReportingEnabled,
      base::Bind(&ArcSupportHost::OnMetricsPreferenceChanged,
                 base::Unretained(this)));
}

ArcSupportHost::~ArcSupportHost() {
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  if (arc_auth_service)
    arc_auth_service->RemoveObserver(this);
}

void ArcSupportHost::Start(Client* client) {
  DCHECK(!client_);
  client_ = client;

  Initialize();
  SendMetricsMode();

  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);
  OnOptInUIShowPage(arc_auth_service->ui_page(),
                    arc_auth_service->ui_page_status());
}

void ArcSupportHost::Initialize() {
  DCHECK(client_);
  std::unique_ptr<base::DictionaryValue> localized_strings(
      new base::DictionaryValue());
  base::string16 device_name = ash::GetChromeOSDeviceName();
  localized_strings->SetString(
      "greetingHeader",
      l10n_util::GetStringFUTF16(IDS_ARC_OPT_IN_DIALOG_HEADER, device_name));
  localized_strings->SetString(
      "greetingDescription",
      l10n_util::GetStringFUTF16(IDS_ARC_OPT_IN_DIALOG_DESCRIPTION,
                                 device_name));
  localized_strings->SetString(
      "buttonAgree",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE));
  localized_strings->SetString(
      "buttonCancel",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_CANCEL));
  localized_strings->SetString(
      "buttonRetry",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_RETRY));
  localized_strings->SetString(
      "progressLsoLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_LSO));
  localized_strings->SetString(
      "progressAndroidLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_ANDROID));
  localized_strings->SetString(
      "authorizationFailed",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_AUTHORIZATION_FAILED));
  localized_strings->SetString(
      "termsOfService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_TERMS_OF_SERVICE));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  const std::string& country_code = base::CountryCodeForCurrentTimezone();
  localized_strings->SetString("countryCode", country_code);

  webui::SetLoadTimeDataDefaults(app_locale, localized_strings.get());

  base::DictionaryValue request;
  std::string request_string;
  request.SetString(kAction, kActionInitialize);
  request.Set(kData, std::move(localized_strings));
  base::JSONWriter::Write(request, &request_string);
  client_->PostMessageFromNativeHost(request_string);
}

void ArcSupportHost::OnMetricsPreferenceChanged() {
  SendMetricsMode();
}

void ArcSupportHost::SendMetricsMode() {
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);
  const Profile* profile = arc_auth_service->profile();

  const bool metrics_managed = IsMetricsReportingPolicyManaged();
  const bool owner_profile =
      profile && chromeos::ProfileHelper::IsOwnerProfile(profile);
  const bool metrics_on =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  int message_id;
  if (metrics_managed || !owner_profile) {
    message_id = metrics_on ? IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED
                            : IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED;
  } else {
    message_id = metrics_on ? IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED
                            : IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED;
  }

  base::DictionaryValue request;
  std::string request_string;
  request.SetString(kAction, kActionSetMetricsMode);
  request.SetString(kText, l10n_util::GetStringUTF16(message_id));
  request.SetBoolean(kCanEnable,
                     !metrics_on && !metrics_managed && owner_profile);
  request.SetBoolean(kOn, metrics_on);
  base::JSONWriter::Write(request, &request_string);
  client_->PostMessageFromNativeHost(request_string);
}

void ArcSupportHost::OnOptInUIClose() {
  if (!client_)
    return;

  base::DictionaryValue response;
  response.SetString(kAction, kActionCloseUI);
  std::string response_string;
  base::JSONWriter::Write(response, &response_string);
  client_->PostMessageFromNativeHost(response_string);
}

void ArcSupportHost::OnOptInUIShowPage(arc::ArcAuthService::UIPage page,
                                       const base::string16& status) {
  if (!client_)
    return;

  base::DictionaryValue response;
  response.SetString(kAction, kActionShowPage);
  response.SetInteger(kPage, static_cast<int>(page));
  response.SetString(kStatus, status);
  std::string response_string;
  base::JSONWriter::Write(response, &response_string);
  client_->PostMessageFromNativeHost(response_string);
}

void ArcSupportHost::EnableMetrics() {
  InitiateMetricsReportingChange(true, OnMetricsReportingCallbackType());
}

void ArcSupportHost::OnMessage(const std::string& request_string) {
  std::unique_ptr<base::Value> request_value =
      base::JSONReader::Read(request_string);
  base::DictionaryValue* request;
  if (!request_value || !request_value->GetAsDictionary(&request)) {
    NOTREACHED();
    return;
  }

  std::string action;
  if (!request->GetString(kAction, &action)) {
    NOTREACHED();
    return;
  }

  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);

  if (action == kActionStartLso) {
    arc_auth_service->StartLso();
  } else if (action == kActionSetAuthCode) {
    std::string code;
    if (!request->GetString(kCode, &code)) {
      NOTREACHED();
      return;
    }
    arc_auth_service->SetAuthCodeAndStartArc(code);
  } else if (action == kActionCancelAuthCode) {
    arc_auth_service->CancelAuthCode();
  } else if (action == kActionEnableMetrics) {
    EnableMetrics();
  } else {
    NOTREACHED();
  }
}

scoped_refptr<base::SingleThreadTaskRunner> ArcSupportHost::task_runner()
    const {
  return base::ThreadTaskRunnerHandle::Get();
}
