// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OPTIONS_OPTIONS_UTIL_H_
#define CHROME_BROWSER_UI_OPTIONS_OPTIONS_UTIL_H_

#include "base/basictypes.h"

class OptionsUtil {
 public:
  // Try to make the the crash stats consent and the metrics upload
  // permission match |enabled|, returns the actual enabled setting.
  static bool ResolveMetricsReportingEnabled(bool enabled);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OptionsUtil);
};

#endif  // CHROME_BROWSER_UI_OPTIONS_OPTIONS_UTIL_H_
