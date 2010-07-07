// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_UTILS_MAC_H_
#define CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_UTILS_MAC_H_

#include "base/basictypes.h"

// Chrome advanced options utility methods for Mac OS X.
class AdvancedOptionsUtilities {
 public:
  // Invoke the Mac OS X Network panel for configuring proxy settings.
  static void ShowNetworkProxySettings();

  // Invoke the Mac OS X Keychain application for inspecting certificates.
  static void ShowManageSSLCertificates();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AdvancedOptionsUtilities);
};

#endif  // CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_UTILS_MAC_H_
