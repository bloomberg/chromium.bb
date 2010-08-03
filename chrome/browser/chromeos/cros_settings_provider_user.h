// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_USER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_USER_H_
#pragma once

#include "base/singleton.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"

namespace chromeos {

class UserCrosSettingsProvider : public CrosSettingsProvider {
 public:
  virtual void Set(const std::wstring& path, Value* in_value);
  virtual bool Get(const std::wstring& path, Value** out_value) const;
  virtual bool HandlesSetting(const std::wstring& path);
  UserCrosSettingsProvider();

 private:
  scoped_ptr<DictionaryValue> dict_;

  DISALLOW_COPY_AND_ASSIGN(UserCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_USER_H_
