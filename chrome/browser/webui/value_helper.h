// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_VALUE_HELPER_H_
#define CHROME_BROWSER_WEBUI_VALUE_HELPER_H_
#pragma once

#include "chrome/browser/sessions/tab_restore_service.h"

#include "base/values.h"

// Used to convert TabRestoreService elements to values for JSON processing.
class ValueHelper {
 public:
  static bool TabToValue(const TabRestoreService::Tab& tab,
      DictionaryValue* dictionary);
  static bool WindowToValue(const TabRestoreService::Window& window,
      DictionaryValue* dictionary);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ValueHelper);
};

#endif  // CHROME_BROWSER_WEBUI_VALUE_HELPER_H_

