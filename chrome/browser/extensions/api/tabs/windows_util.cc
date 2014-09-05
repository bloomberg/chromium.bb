// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"

namespace windows_util {

bool GetWindowFromWindowID(ChromeUIThreadExtensionFunction* function,
                           int window_id,
                           extensions::WindowController** controller) {
  ChromeExtensionFunctionDetails function_details(function);
  if (window_id == extension_misc::kCurrentWindowId) {
    extensions::WindowController* extension_window_controller =
        function->dispatcher()->delegate()->GetExtensionWindowController();
    // If there is a window controller associated with this extension, use that.
    if (extension_window_controller) {
      *controller = extension_window_controller;
    } else {
      // Otherwise get the focused or most recently added window.
      *controller = extensions::WindowControllerList::GetInstance()
                        ->CurrentWindowForFunction(function_details);
    }
    if (!(*controller)) {
      function->SetError(extensions::tabs_constants::kNoCurrentWindowError);
      return false;
    }
  } else {
    *controller = extensions::WindowControllerList::GetInstance()
                      ->FindWindowForFunctionById(function_details, window_id);
    if (!(*controller)) {
      function->SetError(extensions::ErrorUtils::FormatErrorMessage(
          extensions::tabs_constants::kWindowNotFoundError,
          base::IntToString(window_id)));
      return false;
    }
  }
  return true;
}

}  // namespace windows_util
