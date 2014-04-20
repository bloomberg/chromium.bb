// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_

#include <string>

#include "url/gurl.h"

namespace base {
class Value;
}

namespace autofill {

struct PasswordForm;

// When logging decisions made by password management code about whether to
// offer user-entered credentials for saving or not, do use this class. It
// offers a suite of convenience methods to format and scrub logs. The methods
// have built-in privacy protections (never include a password, scrub URLs), so
// that the result is appropriate for display on the internals page.
//
// To use this class, the method SendLog needs to be overriden to send the logs
// for display as appropriate.
//
// TODO(vabr): Logically, this class belongs to the password_manager component.
// But the PasswordAutofillAgent needs to use it, so until that agent is in a
// third component, shared by autofill and password_manager, this helper needs
// to stay in autofill as well.
class SavePasswordProgressLogger {
 public:
  // IDs of strings allowed in the logs: for security reasons, we only pass the
  // IDs from the renderer, and map them to strings in the browser.
  enum StringID {
    STRING_DECISION_ASK,
    STRING_DECISION_DROP,
    STRING_DECISION_SAVE,
    STRING_METHOD,
    STRING_METHOD_GET,
    STRING_METHOD_POST,
    STRING_METHOD_EMPTY,
    STRING_OTHER,
    STRING_SCHEME_HTML,
    STRING_SCHEME_BASIC,
    STRING_SCHEME_DIGEST,
    STRING_SCHEME_MESSAGE,
    STRING_SIGNON_REALM,
    STRING_ORIGINAL_SIGNON_REALM,
    STRING_ORIGIN,
    STRING_ACTION,
    STRING_USERNAME_ELEMENT,
    STRING_PASSWORD_ELEMENT,
    STRING_PASSWORD_AUTOCOMPLETE_SET,
    STRING_OLD_PASSWORD_ELEMENT,
    STRING_SSL_VALID,
    STRING_PASSWORD_GENERATED,
    STRING_TIMES_USED,
    STRING_USE_ADDITIONAL_AUTHENTICATION,
    STRING_PSL_MATCH,
    STRING_NAME_OR_ID,
    STRING_MESSAGE,
    STRING_INVALID,  // Represents a string returned in a case of an error.
    STRING_MAX = STRING_INVALID
  };

  SavePasswordProgressLogger();
  virtual ~SavePasswordProgressLogger();

  // Call these methods to log information. They sanitize the input and call
  // SendLog to pass it for display.
  void LogPasswordForm(StringID label, const autofill::PasswordForm& form);
  void LogHTMLForm(StringID label,
                   const std::string& name_or_id,
                   const std::string& method,
                   const GURL& action);
  void LogURL(StringID label, const GURL& url);
  void LogBoolean(StringID label, bool truth_value);
  void LogNumber(StringID label, int signed_number);
  void LogNumber(StringID label, size_t unsigned_number);
  void LogMessage(StringID message);

 protected:
  // Sends |log| immediately for display.
  virtual void SendLog(const std::string& log) = 0;

 private:
  // Converts |log| and its |label| to a string and calls SendLog on the result.
  void LogValue(StringID label, const base::Value& log);

  DISALLOW_COPY_AND_ASSIGN(SavePasswordProgressLogger);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_
