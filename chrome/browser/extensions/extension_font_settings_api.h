// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_change_registrar.h"

class ExtensionFontSettingsEventRouter : public content::NotificationObserver {
 public:
  explicit ExtensionFontSettingsEventRouter(Profile* profile);
  virtual ~ExtensionFontSettingsEventRouter();

  void Init();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  PrefChangeRegistrar registrar_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFontSettingsEventRouter);
};

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

// Base class for functions that get a font size.
class GetFontSizeExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the font size preference to get.
  virtual const char* GetPrefName() = 0;
};

// Base class for functions that set a font size.
class SetFontSizeExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;

  // Implementations should return the name of the font size preference to set.
  virtual const char* GetPrefName() = 0;
};

class GetDefaultFontSizeFunction : public GetFontSizeExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class SetDefaultFontSizeFunction : public SetFontSizeExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetDefaultFixedFontSizeFunction : public GetFontSizeExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getDefaultFixedFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class SetDefaultFixedFontSizeFunction : public SetFontSizeExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setDefaultFixedFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class GetMinimumFontSizeFunction : public GetFontSizeExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.getMinimumFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

class SetMinimumFontSizeFunction : public SetFontSizeExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.fontSettings.setMinimumFontSize")

 protected:
  virtual const char* GetPrefName() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FONT_SETTINGS_API_H__
