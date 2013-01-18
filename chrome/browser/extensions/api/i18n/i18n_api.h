// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_

#include "chrome/browser/extensions/extension_function.h"

class I18nGetAcceptLanguagesFunction : public SyncExtensionFunction {
  virtual ~I18nGetAcceptLanguagesFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("i18n.getAcceptLanguages", I18N_GETACCEPTLANGUAGES)
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
