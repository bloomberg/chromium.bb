// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CONSUMER_MANAGEMENT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CONSUMER_MANAGEMENT_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// Consumer management overlay page UI handler.
class ConsumerManagementHandler : public ::options::OptionsPageUIHandler {
 public:
  ConsumerManagementHandler();
  virtual ~ConsumerManagementHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;

 private:
  // Handles the button click events from the browser options page.
  void HandleEnrollConsumerManagement(const base::ListValue* args);
  void HandleUnenrollConsumerManagement(const base::ListValue* args);

  // Starts the enrollment process.
  void StartEnrollment();

  // Starts the unenrollment process.
  void StartUnenrollment();

  // Updates the options page UI with the enrollment status.
  void SetEnrollmentStatus(bool is_enrolled);

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CONSUMER_MANAGEMENT_HANDLER_H_
