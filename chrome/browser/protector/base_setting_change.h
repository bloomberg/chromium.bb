// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

class TemplateURL;

namespace protector {

class Protector;

// Base class for setting change tracked by Protector.
class BaseSettingChange {
 public:
  BaseSettingChange();
  virtual ~BaseSettingChange();

  // Applies initial actions to the setting if needed. Must be called before
  // any other calls are made, including text getters.
  // Returns true if initialization was successful. Otherwise, no other
  // calls should be made.
  // Associates this change with |protector_| instance so overrides must
  // call the base method.
  virtual bool Init(Protector* protector);

  // Persists new setting if needed.
  virtual void Apply();

  // Restores old setting if needed.
  virtual void Discard();

  // Indicates that user has ignored this change and timeout has passed.
  virtual void Timeout();

  // Called before the change is removed from the protector instance.
  virtual void OnBeforeRemoved() = 0;

  // Returns the wrench menu item and bubble title.
  virtual string16 GetBubbleTitle() const = 0;

  // Returns the bubble message text.
  virtual string16 GetBubbleMessage() const = 0;

  // Returns text for the button to apply the change with |Apply|.
  // Returns empty string if no apply button should be shown.
  virtual string16 GetApplyButtonText() const = 0;

  // Returns text for the button to discard the change with |Discard|.
  virtual string16 GetDiscardButtonText() const = 0;

  // Protector instance we've been associated with by an |Init| call.
  Protector* protector() { return protector_; }

 private:
  Protector* protector_;

  DISALLOW_COPY_AND_ASSIGN(BaseSettingChange);
};

// TODO(ivankr): CompositeSettingChange that incapsulates multiple
// BaseSettingChange instances.

// Allocates and initializes SettingChange implementation for default search
// provider setting. Both |actual| and |backup| may be NULL if corresponding
// values are unknown or invalid.
BaseSettingChange* CreateDefaultSearchProviderChange(
    const TemplateURL* actual,
    const TemplateURL* backup);

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
