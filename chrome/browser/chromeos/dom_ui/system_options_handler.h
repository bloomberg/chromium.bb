// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_SYSTEM_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_SYSTEM_OPTIONS_HANDLER_H_

#include <string>
#include <vector>

#include "chrome/browser/dom_ui/options_ui.h"
#include "third_party/icu/public/i18n/unicode/timezone.h"

// ChromeOS system options page UI handler.
class SystemOptionsHandler : public OptionsPageUIHandler {
 public:
  SystemOptionsHandler();
  virtual ~SystemOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  // Creates the map of timezones used by the options page.
  ListValue* GetTimezoneList();

  // Gets timezone name.
  std::wstring GetTimezoneName(const icu::TimeZone* timezone);

  // Gets timezone ID which is also used as timezone pref value.
  std::wstring GetTimezoneID(const icu::TimeZone* timezone);

  // Timezones.
  std::vector<icu::TimeZone*> timezones_;

  DISALLOW_COPY_AND_ASSIGN(SystemOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_SYSTEM_OPTIONS_HANDLER_H_
