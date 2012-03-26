// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class GetFontNameFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.getFontName")
};

class SetFontNameFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.setFontName")
};

class GetFontListFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.fontSettings.getFontList")

 private:
  void FontListHasLoaded(scoped_ptr<base::ListValue> list);
  bool CopyFontsToResult(base::ListValue* fonts);
};

class GetDefaultFontSizeFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFontSize")
};

class SetDefaultFontSizeFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFontSize")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
