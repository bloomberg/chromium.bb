// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_NATIVE_DISPLAY_OBSERVER_H_
#define CHROMEOS_DISPLAY_NATIVE_DISPLAY_OBSERVER_H_

namespace chromeos {

// Observer class used by NativeDisplayDelegate to announce when the display
// configuration changes.
class NativeDisplayObserver {
 public:
  virtual ~NativeDisplayObserver() {}

  virtual void OnConfigurationChanged() = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_NATIVE_DISPLAY_OBSERVER_H_
