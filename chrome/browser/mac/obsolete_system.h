// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_OBSOLETE_SYSTEM_H_
#define CHROME_BROWSER_MAC_OBSOLETE_SYSTEM_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"

class ObsoleteSystemMac {
 public:
  // true if 32-bit-only systems are already considered obsolete, or if
  // they'll be considered obsolete soon. Used to control whether to show
  // messaging about 32-bit deprecation within the app.
  static bool Is32BitObsoleteNowOrSoon() {
#if defined(GOOGLE_CHROME_BUILD)
    return true;
#else
    return false;
#endif
  }

  // true if the system's CPU is 32-bit-only, false if it's 64-bit-capable.
#if !defined(ARCH_CPU_64_BITS)
  static bool Has32BitOnlyCPU();
#else
  static bool Has32BitOnlyCPU() {
    return false;
  }
#endif

  // Returns a localized string informing users that their system will either
  // soon be unsupported by future versions of the application, or that they
  // are already using the last version of the application that supports their
  // system. Do not use the returned string unless both
  // Is32BitObsoleteNowOrSoon() and Has32BitOnlyCPU() return true.
#if !defined(ARCH_CPU_64_BITS)
  static base::string16 LocalizedObsoleteSystemString();
#else
  static base::string16 LocalizedObsoleteSystemString() {
    return base::string16();
  }
#endif

  // true if this is the final release that will run on 32-bit-only systems.
  static bool Is32BitEndOfTheLine() {
    // TODO(mark): Change to true immediately prior to the final build that
    // supports 32-bit-only systems.
    return false;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ObsoleteSystemMac);
};

#endif  // CHROME_BROWSER_MAC_OBSOLETE_SYSTEM_H_
