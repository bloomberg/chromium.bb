// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/save_password_progress_logger.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"

using base::checked_cast;
using base::Value;
using base::DictionaryValue;
using base::FundamentalValue;
using base::StringValue;

namespace autofill {

namespace {

// Note 1: Caching the ID->string map in an array would be probably faster, but
// the switch statement is (a) robust against re-ordering, and (b) checks in
// compile-time, that all IDs get a string assigned. The expected frequency of
// calls is low enough (in particular, zero if password manager internals page
// is not open), that optimizing for code robustness is preferred against speed.
// Note 2: The messages can be used as dictionary keys. Do not use '.' in them.
std::string GetStringFromID(SavePasswordProgressLogger::StringID id) {
  switch (id) {
    case SavePasswordProgressLogger::STRING_DECISION_ASK:
      return "Decision: ASK the user";
    case SavePasswordProgressLogger::STRING_DECISION_DROP:
      return "Decision: DROP the password";
    case SavePasswordProgressLogger::STRING_DECISION_SAVE:
      return "Decision: SAVE the password";
    case SavePasswordProgressLogger::STRING_OTHER:
      return "(other)";
    case SavePasswordProgressLogger::STRING_SCHEME_HTML:
      return "HTML";
    case SavePasswordProgressLogger::STRING_SCHEME_BASIC:
      return "Basic";
    case SavePasswordProgressLogger::STRING_SCHEME_DIGEST:
      return "Digest";
    case SavePasswordProgressLogger::STRING_SCHEME_MESSAGE:
      return "Scheme";
    case SavePasswordProgressLogger::STRING_SIGNON_REALM:
      return "Signon realm";
    case SavePasswordProgressLogger::STRING_ORIGINAL_SIGNON_REALM:
      return "Original signon realm";
    case SavePasswordProgressLogger::STRING_ORIGIN:
      return "Origin";
    case SavePasswordProgressLogger::STRING_ACTION:
      return "Action";
    case SavePasswordProgressLogger::STRING_USERNAME_ELEMENT:
      return "Username element";
    case SavePasswordProgressLogger::STRING_PASSWORD_ELEMENT:
      return "Password element";
    case SavePasswordProgressLogger::STRING_PASSWORD_AUTOCOMPLETE_SET:
      return "Password autocomplete set";
    case SavePasswordProgressLogger::STRING_NEW_PASSWORD_ELEMENT:
      return "New password element";
    case SavePasswordProgressLogger::STRING_SSL_VALID:
      return "SSL valid";
    case SavePasswordProgressLogger::STRING_PASSWORD_GENERATED:
      return "Password generated";
    case SavePasswordProgressLogger::STRING_TIMES_USED:
      return "Times used";
    case SavePasswordProgressLogger::STRING_USE_ADDITIONAL_AUTHENTICATION:
      return "Use additional authentication";
    case SavePasswordProgressLogger::STRING_PSL_MATCH:
      return "PSL match";
    case SavePasswordProgressLogger::STRING_NAME_OR_ID:
      return "Form name or ID";
    case SavePasswordProgressLogger::STRING_MESSAGE:
      return "Message";
    case SavePasswordProgressLogger::STRING_SET_AUTH_METHOD:
      return "LoginHandler::SetAuth";
    case SavePasswordProgressLogger::STRING_AUTHENTICATION_HANDLED:
      return "Authentication already handled";
    case SavePasswordProgressLogger::STRING_LOGINHANDLER_FORM:
      return "LoginHandler reports this form";
    case SavePasswordProgressLogger::STRING_SEND_PASSWORD_FORMS_METHOD:
      return "PasswordAutofillAgent::SendPasswordForms";
    case SavePasswordProgressLogger::STRING_SECURITY_ORIGIN:
      return "Security origin";
    case SavePasswordProgressLogger::STRING_SECURITY_ORIGIN_FAILURE:
      return "Security origin cannot access password manager.";
    case SavePasswordProgressLogger::STRING_WEBPAGE_EMPTY:
      return "Webpage is empty.";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_ALL_FORMS:
      return "Number of all forms";
    case SavePasswordProgressLogger::STRING_FORM_FOUND_ON_PAGE:
      return "Form found on page";
    case SavePasswordProgressLogger::STRING_FORM_IS_VISIBLE:
      return "Form is visible";
    case SavePasswordProgressLogger::STRING_FORM_IS_PASSWORD:
      return "Form is a password form";
    case SavePasswordProgressLogger::STRING_WILL_SUBMIT_FORM_METHOD:
      return "PasswordAutofillAgent::WillSubmitForm";
    case SavePasswordProgressLogger::STRING_HTML_FORM_FOR_SUBMIT:
      return "HTML form for submit";
    case SavePasswordProgressLogger::STRING_CREATED_PASSWORD_FORM:
      return "Created PasswordForm";
    case SavePasswordProgressLogger::STRING_SUBMITTED_PASSWORD_REPLACED:
      return "Submitted password replaced with the provisionally saved one.";
    case SavePasswordProgressLogger::STRING_DID_START_PROVISIONAL_LOAD_METHOD:
      return "PasswordAutofillAgent::DidStartProvisionalLoad";
    case SavePasswordProgressLogger::STRING_FORM_FRAME_EQ_FRAME:
      return "form_frame == frame";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVED_FORM_FOR_FRAME:
      return "provisionally_saved_forms_[form_frame]";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_FOUND_ON_PAGE:
      return "PasswordForm found on the page";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVE_PASSWORD_METHOD:
      return "PasswordManager::ProvisionallySavePassword";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVE_PASSWORD_FORM:
      return "ProvisionallySavePassword form";
    case SavePasswordProgressLogger::STRING_IS_SAVING_ENABLED:
      return "IsSavingEnabledForCurrentPage";
    case SavePasswordProgressLogger::STRING_EMPTY_PASSWORD:
      return "Empty password";
    case SavePasswordProgressLogger::STRING_EXACT_MATCH:
      return "Form manager found, exact match.";
    case SavePasswordProgressLogger::STRING_MATCH_WITHOUT_ACTION:
      return "Form manager found, match except for action.";
    case SavePasswordProgressLogger::STRING_MATCHING_NOT_COMPLETE:
      return "No form manager has completed matching.";
    case SavePasswordProgressLogger::STRING_FORM_BLACKLISTED:
      return "Form blacklisted.";
    case SavePasswordProgressLogger::STRING_INVALID_FORM:
      return "Invalid form.";
    case SavePasswordProgressLogger::STRING_AUTOCOMPLETE_OFF:
      return "Autocomplete=off.";
    case SavePasswordProgressLogger::STRING_SYNC_CREDENTIAL:
      return "Credential is used for syncing passwords.";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVED_FORM:
      return "provisionally_saved_form";
    case SavePasswordProgressLogger::STRING_IGNORE_POSSIBLE_USERNAMES:
      return "Ignore other possible usernames";
    case SavePasswordProgressLogger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD:
      return "PasswordManager::OnPasswordFormsRendered";
    case SavePasswordProgressLogger::STRING_NO_PROVISIONAL_SAVE_MANAGER:
      return "No provisional save manager";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_VISIBLE_FORMS:
      return "Number of visible forms";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_REAPPEARED:
      return "Password form re-appeared";
    case SavePasswordProgressLogger::STRING_SAVING_DISABLED:
      return "Saving disabled";
    case SavePasswordProgressLogger::STRING_NO_MATCHING_FORM:
      return "No matching form";
    case SavePasswordProgressLogger::STRING_SSL_ERRORS_PRESENT:
      return "SSL errors present";
    case SavePasswordProgressLogger::STRING_ONLY_VISIBLE:
      return "only_visible";
    case SavePasswordProgressLogger::STRING_INVALID:
      return "INVALID";
      // Intentionally no default: clause here -- all IDs need to get covered.
  }
  NOTREACHED();  // Win compilers don't believe this is unreachable.
  return std::string();
};

// Removes privacy sensitive parts of |url| (currently all but host and scheme).
std::string ScrubURL(const GURL& url) {
  if (url.is_valid())
    return url.GetWithEmptyPath().spec();
  return std::string();
}

// Returns true for all characters which we don't want to see in the logged IDs
// or names of HTML elements.
bool IsUnwantedInElementID(char c) {
  return !(c == '_' || c == '-' || IsAsciiAlpha(c) || IsAsciiDigit(c));
}

// Replaces all characters satisfying IsUnwantedInElementID by a ' ', and turns
// all characters to lowercase. This damages some valid HTML element IDs or
// names, but it is likely that it will be still possible to match the scrubbed
// string to the original ID or name in the HTML doc. That's good enough for the
// logging purposes, and provides some security benefits.
std::string ScrubElementID(std::string element_id) {
  std::replace_if(
      element_id.begin(), element_id.end(), IsUnwantedInElementID, ' ');
  return StringToLowerASCII(element_id);
}

std::string ScrubElementID(const base::string16& element_id) {
  return ScrubElementID(base::UTF16ToUTF8(element_id));
}

std::string FormSchemeToString(PasswordForm::Scheme scheme) {
  SavePasswordProgressLogger::StringID result_id =
      SavePasswordProgressLogger::STRING_INVALID;
  switch (scheme) {
    case PasswordForm::SCHEME_HTML:
      result_id = SavePasswordProgressLogger::STRING_SCHEME_HTML;
      break;
    case PasswordForm::SCHEME_BASIC:
      result_id = SavePasswordProgressLogger::STRING_SCHEME_BASIC;
      break;
    case PasswordForm::SCHEME_DIGEST:
      result_id = SavePasswordProgressLogger::STRING_SCHEME_DIGEST;
      break;
    case PasswordForm::SCHEME_OTHER:
      result_id = SavePasswordProgressLogger::STRING_OTHER;
      break;
  }
  return GetStringFromID(result_id);
}

}  // namespace

SavePasswordProgressLogger::SavePasswordProgressLogger() {
}

SavePasswordProgressLogger::~SavePasswordProgressLogger() {
}

void SavePasswordProgressLogger::LogPasswordForm(
    SavePasswordProgressLogger::StringID label,
    const PasswordForm& form) {
  DictionaryValue log;
  log.SetString(GetStringFromID(STRING_SCHEME_MESSAGE),
                FormSchemeToString(form.scheme));
  log.SetString(GetStringFromID(STRING_SCHEME_MESSAGE),
                FormSchemeToString(form.scheme));
  log.SetString(GetStringFromID(STRING_SIGNON_REALM),
                ScrubURL(GURL(form.signon_realm)));
  log.SetString(GetStringFromID(STRING_ORIGINAL_SIGNON_REALM),
                ScrubURL(GURL(form.original_signon_realm)));
  log.SetString(GetStringFromID(STRING_ORIGIN), ScrubURL(form.origin));
  log.SetString(GetStringFromID(STRING_ACTION), ScrubURL(form.action));
  log.SetString(GetStringFromID(STRING_USERNAME_ELEMENT),
                ScrubElementID(form.username_element));
  log.SetString(GetStringFromID(STRING_PASSWORD_ELEMENT),
                ScrubElementID(form.password_element));
  log.SetBoolean(GetStringFromID(STRING_PASSWORD_AUTOCOMPLETE_SET),
                 form.password_autocomplete_set);
  log.SetString(GetStringFromID(STRING_NEW_PASSWORD_ELEMENT),
                ScrubElementID(form.new_password_element));
  log.SetBoolean(GetStringFromID(STRING_SSL_VALID), form.ssl_valid);
  log.SetBoolean(GetStringFromID(STRING_PASSWORD_GENERATED),
                 form.type == PasswordForm::TYPE_GENERATED);
  log.SetInteger(GetStringFromID(STRING_TIMES_USED), form.times_used);
  log.SetBoolean(GetStringFromID(STRING_USE_ADDITIONAL_AUTHENTICATION),
                 form.use_additional_authentication);
  log.SetBoolean(GetStringFromID(STRING_PSL_MATCH), form.IsPublicSuffixMatch());
  LogValue(label, log);
}

void SavePasswordProgressLogger::LogHTMLForm(
    SavePasswordProgressLogger::StringID label,
    const std::string& name_or_id,
    const GURL& action) {
  DictionaryValue log;
  log.SetString(GetStringFromID(STRING_NAME_OR_ID), ScrubElementID(name_or_id));
  log.SetString(GetStringFromID(STRING_ACTION), ScrubURL(action));
  LogValue(label, log);
}

void SavePasswordProgressLogger::LogURL(
    SavePasswordProgressLogger::StringID label,
    const GURL& url) {
  LogValue(label, StringValue(ScrubURL(url)));
}

void SavePasswordProgressLogger::LogBoolean(
    SavePasswordProgressLogger::StringID label,
    bool truth_value) {
  LogValue(label, FundamentalValue(truth_value));
}

void SavePasswordProgressLogger::LogNumber(
    SavePasswordProgressLogger::StringID label,
    int signed_number) {
  LogValue(label, FundamentalValue(signed_number));
}

void SavePasswordProgressLogger::LogNumber(
    SavePasswordProgressLogger::StringID label,
    size_t unsigned_number) {
  int signed_number = checked_cast<int, size_t>(unsigned_number);
  LogNumber(label, signed_number);
}

void SavePasswordProgressLogger::LogMessage(
    SavePasswordProgressLogger::StringID message) {
  LogValue(STRING_MESSAGE, StringValue(GetStringFromID(message)));
}

void SavePasswordProgressLogger::LogValue(StringID label, const Value& log) {
  std::string log_string;
  bool conversion_to_string_successful = base::JSONWriter::WriteWithOptions(
      &log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &log_string);
  DCHECK(conversion_to_string_successful);
  SendLog(GetStringFromID(label) + ": " + log_string);
}

}  // namespace autofill
