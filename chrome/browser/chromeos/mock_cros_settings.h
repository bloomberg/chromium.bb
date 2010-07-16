// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MOCK_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_MOCK_CROS_SETTINGS_H_

#include "base/singleton.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros_settings.h"

namespace chromeos {

class MockCrosSettings : public CrosSettings {
 public:
  virtual void Set(const std::wstring& path, Value* in_value);
  virtual bool Get(const std::wstring& path, Value** out_value) const;

 private:
  scoped_ptr<DictionaryValue> dict_;

  MockCrosSettings();
  friend struct DefaultSingletonTraits<MockCrosSettings>;

  DISALLOW_COPY_AND_ASSIGN(MockCrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MOCK_CROS_SETTINGS_H_
