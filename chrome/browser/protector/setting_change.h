// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_SETTING_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_SETTING_CHANGE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

class TemplateURL;

namespace protector {

class Protector;

// Base class for setting change tracked by Protector.
class SettingChange {
 public:
  SettingChange() {}
  virtual ~SettingChange() {}

  // Applies initial actions to the setting if needed. Must be called before
  // any other calls are made, including text getters. Returns true if
  // initialization was successful.
  virtual bool Init(Protector* protector) { return true; }

  // Persists new setting if needed.
  virtual void Apply(Protector* protector) {}

  // Restores old setting if needed.
  virtual void Discard(Protector* protector) {}

  // Returns the wrench menu item and bubble title.
  virtual string16 GetTitle() const = 0;

  // Returns the bubble message text.
  virtual string16 GetMessage() const = 0;

  // Returns text for the button to apply the change with |Apply|.
  // Returns empty string if no apply button should be shown.
  virtual string16 GetApplyButtonText() const = 0;

  // Returns text for the button to discard the change with |Discard|.
  virtual string16 GetDiscardButtonText() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingChange);
};

// TODO(ivankr): CompositeSettingChange that incapsulates multiple
// SettingChange instances.

// Allocates and initializes SettingChange implementation for default search
// provider setting. Both |actual| and |backup| may be NULL if corresponding
// values are unknown or invalid.
SettingChange* CreateDefaultSearchProviderChange(
    const TemplateURL* actual,
    const TemplateURL* backup);

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_SETTING_CHANGE_H_
