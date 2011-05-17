// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace views {
class Widget;
}  // namespace views

// Base class for input APIs.
class InputFunction : public AsyncExtensionFunction {
 public:
  virtual void Run();
  virtual bool RunImpl() = 0;
};

// Note that this experimental API is currently only available for
// TOOLKIT_VIEWS (see chrome/chrome_browser.gypi).
//
// We may eventually support other platforms by adding the necessary
// synthetic event distribution code to this Function.
class SendKeyboardEventInputFunction : public InputFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.sendKeyboardEvent");

 private:
  views::Widget* GetTopLevelWidget();
};

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
// Note that this experimental APIs are currently only available for
// TOUCH_UI version of Chrome OS. Please also note that the version of Chrome
// OS is always built with TOOLKIT_VIEWS.
//
// We may eventually support other platforms, especially non TOUCH_UI version
// of Chrome OS.
class SendHandwritingStrokeFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.sendHandwritingStroke");
};

class CancelHandwritingStrokesFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.cancelHandwritingStrokes");
};
#endif

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_API_H_
