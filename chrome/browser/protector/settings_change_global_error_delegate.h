// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_SETTINGS_CHANGE_GLOBAL_ERROR_DELEGATE_H_
#define CHROME_BROWSER_PROTECTOR_SETTINGS_CHANGE_GLOBAL_ERROR_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class Browser;

namespace protector {

class SettingsChangeGlobalError;

// Interface for notifications about settings change error bubble closing.
class SettingsChangeGlobalErrorDelegate {
 public:
  virtual ~SettingsChangeGlobalErrorDelegate() {}

  // Called if user clicks "Apply change" button.
  virtual void OnApplyChange(SettingsChangeGlobalError* error,
                             Browser* browser) = 0;

  // Called if user clicks "Discard change" button.
  virtual void OnDiscardChange(SettingsChangeGlobalError* error,
                               Browser* browser) = 0;

  // Called if user clicked outside the bubble and timeout for its reshow
  // has passed.
  virtual void OnDecisionTimeout(SettingsChangeGlobalError* error) = 0;

  // Called when error is removed from profile so it's safe to delete it.
  virtual void OnRemovedFromProfile(SettingsChangeGlobalError* error) = 0;
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_SETTINGS_CHANGE_GLOBAL_ERROR_DELEGATE_H_
