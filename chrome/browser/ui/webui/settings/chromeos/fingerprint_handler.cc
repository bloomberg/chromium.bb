// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace chromeos {
namespace settings {

FingerprintHandler::FingerprintHandler() {}

FingerprintHandler::~FingerprintHandler() {}

void FingerprintHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getFingerprintsList",
      base::Bind(&FingerprintHandler::HandleGetFingerprintsList,
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
}

void FingerprintHandler::OnJavascriptAllowed() {}

void FingerprintHandler::OnJavascriptDisallowed() {}

void FingerprintHandler::HandleGetFingerprintsList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  // TODO(sammiequon): Send more than empty list.
  base::ListValue fingerprints_list;
  ResolveJavascriptCallback(base::StringValue(callback_id), fingerprints_list);
}

void FingerprintHandler::HandleStartEnroll(const base::ListValue* args) {
}

void FingerprintHandler::HandleCancelCurrentEnroll(
    const base::ListValue* args) {
}

void FingerprintHandler::HandleGetEnrollmentLabel(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  // TODO(sammiequon): Send the proper label.
  ResolveJavascriptCallback(base::StringValue(callback_id),
                            base::StringValue("Label"));
}

void FingerprintHandler::HandleRemoveEnrollment(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  int index;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetInteger(1, &index));

  // TODO(sammiequon): Send more than empty list.
  base::ListValue fingerprints_list;
  ResolveJavascriptCallback(base::StringValue(callback_id), fingerprints_list);
}

void FingerprintHandler::HandleChangeEnrollmentLabel(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  std::string new_label;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetString(1, &new_label));

  // TODO(sammiequon): Send the proper label.
  ResolveJavascriptCallback(base::StringValue(callback_id),
                            base::StringValue("Label"));
}

void FingerprintHandler::HandleStartAuthentication(
    const base::ListValue* args) {
}

void FingerprintHandler::HandleEndCurrentAuthentication(
    const base::ListValue* args) {
}

}  // namespace settings
}  // namespace chromeos
