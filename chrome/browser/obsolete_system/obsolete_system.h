// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OBSOLETE_SYSTEM_OBSOLETE_SYSTEM_H_
#define CHROME_BROWSER_OBSOLETE_SYSTEM_OBSOLETE_SYSTEM_H_

#include "base/macros.h"
#include "base/strings/string16.h"

class ObsoleteSystem {
 public:
  // true if the system is already considered obsolete, or if it'll be
  // considered obsolete soon. Used to control whether to show messaging about
  // deprecation within the app.
  static bool IsObsoleteNowOrSoon();

  // Returns a localized string informing users that their system will either
  // soon be unsupported by future versions of the application, or that they
  // are already using the last version of the application that supports their
  // system. Do not use the returned string unless IsObsoleteNowOrSoon() returns
  // true.
  static base::string16 LocalizedObsoleteString();

  // true if this is the final release. This is only valid when
  // IsObsoleteNowOrSoon() returns true.
  static bool IsEndOfTheLine();

  // A help URL to explain the deprecation. Do not use the returned string
  // unless IsObsoleteNowOrSoon() returns true.
  static const char* GetLinkURL();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ObsoleteSystem);
};

#endif  // CHROME_BROWSER_OBSOLETE_SYSTEM_OBSOLETE_SYSTEM_H_
