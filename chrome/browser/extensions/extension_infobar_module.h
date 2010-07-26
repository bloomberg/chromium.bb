// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_MODULE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_MODULE_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class ShowInfoBarFunction : public SyncExtensionFunction {
  ~ShowInfoBarFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.infobars.show")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_MODULE_H_
