// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

// The max number of fingerprints that can be stored.
const int kMaxAllowedFingerprints = 5;

std::unique_ptr<base::DictionaryValue> GetFingerprintsInfo(
    const std::vector<std::string>& fingerprints_list) {
  auto response = base::MakeUnique<base::DictionaryValue>();
  auto fingerprints = base::MakeUnique<base::ListValue>();

  DCHECK_LE(static_cast<int>(fingerprints_list.size()),
            kMaxAllowedFingerprints);
  for (auto& fingerprint_name: fingerprints_list) {
    std::unique_ptr<base::Value> str =
        base::MakeUnique<base::Value>(fingerprint_name);
    fingerprints->Append(std::move(str));
  }

  response->Set("fingerprintsList", std::move(fingerprints));
  response->SetBoolean("isMaxed", static_cast<int>(fingerprints_list.size()) >=
                                      kMaxAllowedFingerprints);
  return response;
}

}  // namespace

FingerprintHandler::FingerprintHandler(Profile* profile) : profile_(profile) {}

FingerprintHandler::~FingerprintHandler() {}

void FingerprintHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getFingerprintsList",
      base::Bind(&FingerprintHandler::HandleGetFingerprintsList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNumFingerprints",
      base::Bind(&FingerprintHandler::HandleGetNumFingerprints,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startEnroll",
      base::Bind(&FingerprintHandler::HandleStartEnroll,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelCurrentEnroll",
      base::Bind(&FingerprintHandler::HandleCancelCurrentEnroll,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getEnrollmentLabel",
      base::Bind(&FingerprintHandler::HandleGetEnrollmentLabel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeEnrollment",
      base::Bind(&FingerprintHandler::HandleRemoveEnrollment,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "changeEnrollmentLabel",
      base::Bind(&FingerprintHandler::HandleChangeEnrollmentLabel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startAuthentication",
      base::Bind(&FingerprintHandler::HandleStartAuthentication,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "endCurrentAuthentication",
      base::Bind(&FingerprintHandler::HandleEndCurrentAuthentication,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fakeScanComplete",
      base::Bind(&FingerprintHandler::HandleFakeScanComplete,
                 base::Unretained(this)));
}

void FingerprintHandler::OnJavascriptAllowed() {}

void FingerprintHandler::OnJavascriptDisallowed() {}

void FingerprintHandler::HandleGetFingerprintsList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::unique_ptr<base::DictionaryValue> fingerprint_info =
      GetFingerprintsInfo(fingerprints_list_);
  ResolveJavascriptCallback(base::Value(callback_id), *fingerprint_info);
}

void FingerprintHandler::HandleGetNumFingerprints(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(static_cast<int>(fingerprints_list_.size())));
}

void FingerprintHandler::HandleStartEnroll(const base::ListValue* args) {
}

void FingerprintHandler::HandleCancelCurrentEnroll(
    const base::ListValue* args) {
}

void FingerprintHandler::HandleGetEnrollmentLabel(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  int index;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetInteger(1, &index));

  DCHECK_LT(index, static_cast<int>(fingerprints_list_.size()));
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(fingerprints_list_[index]));
}

void FingerprintHandler::HandleRemoveEnrollment(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  int index;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetInteger(1, &index));

  DCHECK_LT(index, static_cast<int>(fingerprints_list_.size()));
  bool deleteSucessful = true;
  fingerprints_list_.erase(fingerprints_list_.begin() + index);
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(deleteSucessful));
}

void FingerprintHandler::HandleChangeEnrollmentLabel(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  int index;
  std::string new_label;
  CHECK(args->GetInteger(0, &index));
  CHECK(args->GetString(1, &new_label));

  fingerprints_list_[index] = new_label;
}

void FingerprintHandler::HandleStartAuthentication(
    const base::ListValue* args) {
}

void FingerprintHandler::HandleEndCurrentAuthentication(
    const base::ListValue* args) {
}

void FingerprintHandler::HandleFakeScanComplete(
    const base::ListValue* args) {
  DCHECK_LT(static_cast<int>(fingerprints_list_.size()),
            kMaxAllowedFingerprints);
  // Determines what the newly added fingerprint's name should be.
  for (int i = 1; i <= kMaxAllowedFingerprints; ++i) {
    std::string fingerprint_name = l10n_util::GetStringFUTF8(
        IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME,
        base::IntToString16(i));
    if (std::find(fingerprints_list_.begin(), fingerprints_list_.end(),
                  fingerprint_name) == fingerprints_list_.end()) {
      fingerprints_list_.push_back(fingerprint_name);
      break;
    }
  }
}

void FingerprintHandler::OnAttemptReceived(int result,
                                           const AttemptMatches& matches) {
  // When the user touches the sensor, highlight the label(s) that finger is
  // associated with, if it is registered with this user.
  auto it = matches.find(profile_->GetProfileUserName());
  if (it == matches.end() || it->second.size() < 1)
    return;

  auto fingerprint_attempt = base::MakeUnique<base::DictionaryValue>();
  auto user_ids = base::MakeUnique<base::ListValue>();

  for (const std::string& matched_label : it->second) {
    auto label_it = std::find(fingerprints_list_.begin(),
                              fingerprints_list_.end(), matched_label);
    DCHECK(label_it != fingerprints_list_.end());
    user_ids->AppendInteger(
        static_cast<int>(label_it - fingerprints_list_.begin()));
  }

  fingerprint_attempt->SetInteger("result", result);
  fingerprint_attempt->Set("indexes", std::move(user_ids));

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("on-fingerprint-attempt-received"),
                         *fingerprint_attempt);
}

void FingerprintHandler::OnScanReceived(int result, bool is_complete) {
  auto scan_attempt = base::MakeUnique<base::DictionaryValue>();
  scan_attempt->SetInteger("result", result);
  scan_attempt->SetBoolean("isComplete", is_complete);

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("on-fingerprint-scan-received"),
                         *scan_attempt);
}

}  // namespace settings
}  // namespace chromeos
