// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PERSONAL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_PERSONAL_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/sync/profile_sync_service.h"

// Chrome personal options page UI handler.
class PersonalOptionsHandler : public OptionsPageUIHandler {
 public:
  PersonalOptionsHandler();
  virtual ~PersonalOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  virtual void SetSyncStatusUIString(const Value* value);

  DISALLOW_COPY_AND_ASSIGN(PersonalOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_PERSONAL_OPTIONS_HANDLER_H_
