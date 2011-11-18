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
  // IDs of changes Protector currently tracks.
  enum Type {
    // Default search engine has been changed.
    kSearchEngineChanged,

    // Home page has been changed.
    kHomePageChanged,
  };

  explicit SettingChange(Type type) : type_(type) {}
  virtual ~SettingChange() {}

  Type type() const { return type_; }

  // Returns the old setting presentation to be shown to user.
  // Returns empty string if the old setting is unavailable.
  virtual string16 GetOldSetting() const = 0;

  // Returns the new setting presentation to be shown to user.
  virtual string16 GetNewSetting() const = 0;

  // Persists new setting if needed.
  virtual void Accept(Protector* protector) {}

  // Restores old setting value if needed.
  virtual void Revert(Protector* protector) {}

  // Called when user ignored the change.
  virtual void DoDefault(Protector* protector) {}

 private:
  // Type of the change. Used for strings lookup by UI.
  // TODO(avayvod): Refactor string selection logic via polymorphism.
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(SettingChange);
};

typedef std::vector<SettingChange*> SettingChangeVector;
typedef void (SettingChange::*SettingChangeAction)(Protector*);

// Allocates and initializes SettingChange implementation for default search
// provider setting.
SettingChange* CreateDefaultSearchProviderChange(
    const TemplateURL* actual,
    const TemplateURL* backup);

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_SETTING_CHANGE_H_
