// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_PREF_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_PREFS_PREF_OBSERVER_BRIDGE_H_

#include <string>

#include "base/compiler_specific.h"

class PrefChangeRegistrar;

@protocol PrefObserverDelegate
- (void)onPreferenceChanged:(const std::string&)preferenceName;
@end

class PrefObserverBridge {
 public:
  explicit PrefObserverBridge(id<PrefObserverDelegate> delegate);
  virtual ~PrefObserverBridge();

  virtual void ObserveChangesForPreference(const std::string& pref_name,
                                           PrefChangeRegistrar* registrar);

 private:
  virtual void OnPreferenceChanged(const std::string& pref_name);

  __unsafe_unretained id<PrefObserverDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_PREFS_PREF_OBSERVER_BRIDGE_H_
