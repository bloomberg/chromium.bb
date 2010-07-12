// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_CONTENT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_CONTENT_SETTINGS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

class ContentSettingsHandler : public OptionsPageUIHandler {
 public:
  ContentSettingsHandler();
  virtual ~ContentSettingsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  virtual void RegisterMessages();

 private:
  void GetContentFilterSettings(const Value* value);
  void SetContentFilter(const Value* value);
  void SetAllowThirdPartyCookies(const Value* value);

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_CONTENT_SETTINGS_HANDLER_H_
