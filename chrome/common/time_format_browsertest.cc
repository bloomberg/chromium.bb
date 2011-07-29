// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This whole test runs as a separate browser_test because it depends on a
// static initialization inside third_party/icu (gDecimal in digitlst.cpp).
//
// That initialization depends on the current locale, and on certain locales
// will lead to wrong behavior. To make sure that the locale is set before
// icu is used, and that the "wrong" static value doesn't affect other tests,
// this test is executed on its own process.

#include "base/string16.h"
#include "base/test/scoped_locale.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/time_format.h"
#include "chrome/test/base/in_process_browser_test.h"

using base::TimeDelta;

class TimeFormatBrowserTest : public InProcessBrowserTest {
 public:
  TimeFormatBrowserTest() : scoped_locale_("fr_FR.utf-8") {
  }

 private:
  base::ScopedLocale scoped_locale_;
};

IN_PROC_BROWSER_TEST_F(TimeFormatBrowserTest, DecimalPointNotDot) {
  // Some locales use a comma ',' instead of a dot '.' as the separator for
  // decimal digits. The icu library wasn't handling this, leading to "1"
  // being internally converted to "+1,0e00" and ultimately leading to "NaN".
  // This showed up on the browser on estimated download time, for example.
  // http://crbug.com/60476

  string16 one_min = TimeFormat::TimeRemainingShort(TimeDelta::FromMinutes(1));
  EXPECT_EQ(ASCIIToUTF16("1 min"), one_min);
}
