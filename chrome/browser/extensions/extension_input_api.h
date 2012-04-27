// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_API_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

// Note that this experimental API is currently only available for
// TOOLKIT_VIEWS (see chrome/chrome_browser.gypi).
//
// We may eventually support other platforms by adding the necessary
// synthetic event distribution code to this Function.
class SendKeyboardEventInputFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.virtualKeyboard.sendKeyboardEvent");

 protected:
  virtual ~SendKeyboardEventInputFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#if defined(USE_VIRTUAL_KEYBOARD)
class HideKeyboardFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.virtualKeyboard.hideKeyboard");

 protected:
  virtual ~HideKeyboardFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetKeyboardHeightFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.virtualKeyboard.setKeyboardHeight");

 protected:
  virtual ~SetKeyboardHeightFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};
#endif

#if defined(OS_CHROMEOS) && defined(USE_VIRTUAL_KEYBOARD)
// Note that these experimental APIs are currently only available for
// versions of Chrome OS built with USE_VIRTUAL_KEYBOARD. Please also note that
// the version of Chrome OS is always built with TOOLKIT_VIEWS.
//
// We may eventually support other platforms, especially versions of ChromeOS
// without USE_VIRTUAL_KEYBOARD.
class SendHandwritingStrokeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.virtualKeyboard.sendHandwritingStroke");

 protected:
  virtual ~SendHandwritingStrokeFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class CancelHandwritingStrokesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.virtualKeyboard.cancelHandwritingStrokes");

 public:
  virtual ~CancelHandwritingStrokesFunction() {}
  virtual bool RunImpl() OVERRIDE;
};
#endif

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_API_H_
