// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/save_password_progress_logger.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"

using base::checked_cast;
using base::Value;
using base::DictionaryValue;
using base::FundamentalValue;
using base::StringValue;

namespace autofill {

namespace {

// Removes privacy sensitive parts of |url| (currently all but host and scheme).
std::string ScrubURL(const GURL& url) {
  if (url.is_valid())
    return url.GetWithEmptyPath().spec();
  return std::string();
}

std::string FormSchemeToString(PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::SCHEME_HTML:
      return "HTML";
    case PasswordForm::SCHEME_BASIC:
      return "BASIC";
    case PasswordForm::SCHEME_DIGEST:
      return "DIGEST";
    case PasswordForm::SCHEME_OTHER:
      return "OTHER";
  }
  NOTREACHED();  // Win compilers don't believe this is unreachable.
  return std::string();
}

StringValue DecisionToStringValue(
    SavePasswordProgressLogger::Decision decision) {
  switch (decision) {
    case SavePasswordProgressLogger::DECISION_SAVE:
      return StringValue("SAVE the password");
    case SavePasswordProgressLogger::DECISION_ASK:
      return StringValue("ASK the user whether to save the password");
    case SavePasswordProgressLogger::DECISION_DROP:
      return StringValue("DROP the password");
  }
  NOTREACHED();  // Win compilers don't believe this is unreachable.
  return StringValue(std::string());
}

}  // namespace

SavePasswordProgressLogger::SavePasswordProgressLogger() {}

SavePasswordProgressLogger::~SavePasswordProgressLogger() {}

void SavePasswordProgressLogger::LogPasswordForm(const std::string& message,
                                                 const PasswordForm& form) {
  DictionaryValue log;
  // Do not use the "<<" operator for PasswordForms, because it also prints
  // passwords. Also, that operator is only for testing.
  log.SetString("scheme", FormSchemeToString(form.scheme));
  log.SetString("signon realm", ScrubURL(GURL(form.signon_realm)));
  log.SetString("original signon realm",
                ScrubURL(GURL(form.original_signon_realm)));
  log.SetString("origin", ScrubURL(form.origin));
  log.SetString("action", ScrubURL(form.action));
  log.SetString("username element", form.username_element);
  log.SetString("password element", form.password_element);
  log.SetBoolean("password autocomplete set", form.password_autocomplete_set);
  log.SetString("old password element", form.old_password_element);
  log.SetBoolean("ssl valid", form.ssl_valid);
  log.SetBoolean("password generated",
                 form.type == PasswordForm::TYPE_GENERATED);
  log.SetInteger("times used", form.times_used);
  log.SetBoolean("use additional authentication",
                 form.use_additional_authentication);
  log.SetBoolean("is PSL match", form.IsPublicSuffixMatch());
  LogValue(message, log);
}

void SavePasswordProgressLogger::LogHTMLForm(const std::string& message,
                                             const std::string& name_or_id,
                                             const std::string& method,
                                             const GURL& action) {
  DictionaryValue log;
  log.SetString("name_or_id", name_or_id);
  log.SetString("method", method);
  log.SetString("action", ScrubURL(action));
  LogValue(message, log);
}

void SavePasswordProgressLogger::LogURL(const std::string& message,
                                        const GURL& url) {
  LogValue(message, StringValue(ScrubURL(url)));
}

void SavePasswordProgressLogger::LogBoolean(const std::string& message,
                                            bool value) {
  LogValue(message, FundamentalValue(value));
}

void SavePasswordProgressLogger::LogNumber(const std::string& message,
                                           int value) {
  LogValue(message, FundamentalValue(value));
}

void SavePasswordProgressLogger::LogNumber(const std::string& message,
                                           size_t value) {
  LogValue(message, FundamentalValue(checked_cast<int, size_t>(value)));
}

void SavePasswordProgressLogger::LogFinalDecision(Decision decision) {
  LogValue("Final decision taken", DecisionToStringValue(decision));
}

void SavePasswordProgressLogger::LogMessage(const std::string& message) {
  LogValue("Message", StringValue(message));
}

void SavePasswordProgressLogger::LogValue(const std::string& name,
                                          const Value& log) {
  std::string log_string;
  bool conversion_to_string_successful = base::JSONWriter::WriteWithOptions(
      &log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &log_string);
  DCHECK(conversion_to_string_successful);
  SendLog(name + ": " + log_string);
}

}  // namespace autofill
