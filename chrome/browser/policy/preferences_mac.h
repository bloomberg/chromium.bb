// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PREFERENCES_MAC_H_
#define CHROME_BROWSER_POLICY_PREFERENCES_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/basictypes.h"

// Wraps a small part of the CFPreferences API surface in a very thin layer, to
// allow it to be mocked out for testing.

// See CFPreferences documentation for function documentation, as these call
// through directly to their CFPreferences equivalents (Foo ->
// CFPreferencesFoo).
class MacPreferences {
 public:
  MacPreferences() {}
  virtual ~MacPreferences() {}

  virtual Boolean AppSynchronize(CFStringRef applicationID);

  virtual CFPropertyListRef CopyAppValue(CFStringRef key,
                                         CFStringRef applicationID);

  virtual Boolean AppValueIsForced(CFStringRef key, CFStringRef applicationID);

 private:
  DISALLOW_COPY_AND_ASSIGN(MacPreferences);
};

#endif  // CHROME_BROWSER_POLICY_PREFERENCES_MAC_H_
