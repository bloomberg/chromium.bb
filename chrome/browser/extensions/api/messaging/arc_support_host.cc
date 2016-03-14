// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/arc_support_host.h"

#include "ash/system/chromeos/devicetype_utils.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace {
const char kAction[] = "action";
const char kStatus[] = "status";
const char kData[] = "data";
const char kPage[] = "page";
const char kActionSetLocalization[] = "setLocalization";
const char kActionCheckAuthCode[] = "checkAuthCode";
const char kActionCancelAuthCode[] = "cancelAuthCode";
const char kActionCloseUI[] = "closeUI";
const char kActionShowPage[] = "showPage";
}  // namespace

// static
const char ArcSupportHost::kHostName[] = "com.google.arc_support";

// static
const char* const ArcSupportHost::kHostOrigin[] = {
    "chrome-extension://cnbgggchhmkkdmeppjobngjoejnihlei/"
};

// static
scoped_ptr<extensions::NativeMessageHost> ArcSupportHost::Create() {
  return scoped_ptr<NativeMessageHost>(new ArcSupportHost());
}

ArcSupportHost::ArcSupportHost() {
  arc::ArcAuthService::Get()->AddObserver(this);
}

ArcSupportHost::~ArcSupportHost() {
  arc::ArcAuthService::Get()->RemoveObserver(this);
}

void ArcSupportHost::Start(Client* client) {
  DCHECK(!client_);
  client_ = client;

  SendLocalization();

  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  OnOptInUIShowPage(arc_auth_service->ui_page(),
                    arc_auth_service->ui_page_status());
}

void ArcSupportHost::SendLocalization() {
  DCHECK(client_);
  scoped_ptr<base::DictionaryValue> localized_strings(
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
      "greetingLegacy",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_LEGACY));
  localized_strings->SetString(
      "buttonGetStarted",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_GET_STARTED));
  localized_strings->SetString(
      "buttonRetry",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_RETRY));
  localized_strings->SetString(
      "progressLSOLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_LSO));
  localized_strings->SetString(
      "progressAndroidLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_ANDROID));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings.get());

  base::DictionaryValue request;
  std::string request_string;
  request.SetString(kAction, kActionSetLocalization);
  request.Set(kData, std::move(localized_strings));
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

void ArcSupportHost::OnMessage(const std::string& request_string) {
  scoped_ptr<base::Value> request_value =
      base::JSONReader::Read(request_string);
  scoped_ptr<base::DictionaryValue> request(
      static_cast<base::DictionaryValue*>(request_value.release()));
  if (!request.get()) {
    NOTREACHED();
    return;
  }

  std::string action;
  if (!request->GetString(kAction, &action)) {
    NOTREACHED();
    return;
  }

  if (action == kActionCheckAuthCode) {
    arc::ArcAuthService::Get()->CheckAuthCode();
  } else if (action == kActionCancelAuthCode) {
    arc::ArcAuthService::Get()->CancelAuthCode();
  } else {
    NOTREACHED();
  }
}

scoped_refptr<base::SingleThreadTaskRunner> ArcSupportHost::task_runner()
    const {
  return base::ThreadTaskRunnerHandle::Get();
}
