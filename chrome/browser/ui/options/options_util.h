// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OPTIONS_OPTIONS_UTIL_H_
#define CHROME_BROWSER_UI_OPTIONS_OPTIONS_UTIL_H_
#pragma once

#include "base/basictypes.h"

// An identifier for an Options Tab page. These are treated as indices into
// the list of available tabs to be displayed. PAGE_DEFAULT means select the
// last tab viewed when the Options window was opened, or PAGE_GENERAL if the
// Options was never opened.
enum OptionsPage {
  OPTIONS_PAGE_DEFAULT = -1,
#if defined(OS_CHROMEOS)
  OPTIONS_PAGE_SYSTEM,
  OPTIONS_PAGE_INTERNET,
#endif
  OPTIONS_PAGE_GENERAL,
  OPTIONS_PAGE_CONTENT,
  OPTIONS_PAGE_ADVANCED,
  OPTIONS_PAGE_COUNT
};

class OptionsUtil {
 public:
  // Try to make the the crash stats consent and the metrics upload
  // permission match |enabled|, returns the actual enabled setting.
  static bool ResolveMetricsReportingEnabled(bool enabled);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OptionsUtil);
};

#endif  // CHROME_BROWSER_UI_OPTIONS_OPTIONS_UTIL_H_
