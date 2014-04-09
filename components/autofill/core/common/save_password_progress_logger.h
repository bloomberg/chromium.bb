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
  // All three possible decisions about saving a password. Call LogFinalDecision
  // as soon as one is taken by the password management code.
  enum Decision { DECISION_SAVE, DECISION_ASK, DECISION_DROP };

  SavePasswordProgressLogger();
  virtual ~SavePasswordProgressLogger();

  // Logging: specialized methods (for logging forms, URLs, etc.) take care of
  // proper removing of sensitive data where appropriate.
  void LogPasswordForm(const std::string& message,
                       const autofill::PasswordForm& form);
  void LogHTMLForm(const std::string& message,
                   const std::string& name_or_id,
                   const std::string& method,
                   const GURL& action);
  void LogURL(const std::string& message, const GURL& url);
  void LogWhetherObjectExists(const std::string& message, const void* object);
  void LogBoolean(const std::string& message, bool value);
  void LogNumber(const std::string& message, int value);
  void LogNumber(const std::string& message, size_t value);
  void LogFinalDecision(Decision decision);
  // Do not use LogMessage when there is an appropriate specialized method
  // above. LogMessage performs no scrubbing of sensitive data.
  void LogMessage(const std::string& message);

 protected:
  // Sends |log| immediately for display.
  virtual void SendLog(const std::string& log) = 0;

 private:
  // Takes a structured |log|, converts it to a string suitable for plain text
  // output, adds the |name| as a caption, and sends out via SendLog.
  void LogValue(const std::string& name, const base::Value& log);

  DISALLOW_COPY_AND_ASSIGN(SavePasswordProgressLogger);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_
