// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class GetPreferenceFunction : public SyncExtensionFunction {
 public:
  virtual ~GetPreferenceFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.preferences.get")
};

class SetPreferenceFunction : public SyncExtensionFunction {
 public:
  virtual ~SetPreferenceFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.preferences.set")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_API_H__
