// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_options.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

TEST(TraceOptionsTest, TraceOptionsFromString) {
  TraceOptions options;
  EXPECT_TRUE(options.SetFromString("record-until-full"));
  EXPECT_EQ(RECORD_UNTIL_FULL, options.record_mode);
  EXPECT_FALSE(options.enable_sampling);
  EXPECT_FALSE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString("record-continuously"));
  EXPECT_EQ(RECORD_CONTINUOUSLY, options.record_mode);
  EXPECT_FALSE(options.enable_sampling);
  EXPECT_FALSE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString("trace-to-console"));
  EXPECT_EQ(ECHO_TO_CONSOLE, options.record_mode);
  EXPECT_FALSE(options.enable_sampling);
  EXPECT_FALSE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString("record-as-much-as-possible"));
  EXPECT_EQ(RECORD_AS_MUCH_AS_POSSIBLE, options.record_mode);
  EXPECT_FALSE(options.enable_sampling);
  EXPECT_FALSE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString("record-until-full, enable-sampling"));
  EXPECT_EQ(RECORD_UNTIL_FULL, options.record_mode);
  EXPECT_TRUE(options.enable_sampling);
  EXPECT_FALSE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString("enable-systrace,record-continuously"));
  EXPECT_EQ(RECORD_CONTINUOUSLY, options.record_mode);
  EXPECT_FALSE(options.enable_sampling);
  EXPECT_TRUE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString(
      "enable-systrace, trace-to-console,enable-sampling"));
  EXPECT_EQ(ECHO_TO_CONSOLE, options.record_mode);
  EXPECT_TRUE(options.enable_sampling);
  EXPECT_TRUE(options.enable_systrace);

  EXPECT_TRUE(options.SetFromString(
      "record-continuously,record-until-full,trace-to-console"));
  EXPECT_EQ(ECHO_TO_CONSOLE, options.record_mode);
  EXPECT_FALSE(options.enable_systrace);
  EXPECT_FALSE(options.enable_sampling);

  EXPECT_TRUE(options.SetFromString(""));
  EXPECT_EQ(RECORD_UNTIL_FULL, options.record_mode);
  EXPECT_FALSE(options.enable_systrace);
  EXPECT_FALSE(options.enable_sampling);

  EXPECT_FALSE(options.SetFromString("foo-bar-baz"));
}

TEST(TraceOptionsTest, TraceOptionsToString) {
  // Test that we can intialize TraceOptions from a string got from
  // TraceOptions.ToString() method to get a same TraceOptions.
  TraceRecordMode modes[] = {RECORD_UNTIL_FULL,
                             RECORD_CONTINUOUSLY,
                             ECHO_TO_CONSOLE,
                             RECORD_AS_MUCH_AS_POSSIBLE};
  bool enable_sampling_options[] = {true, false};
  bool enable_systrace_options[] = {true, false};

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        TraceOptions original_option = TraceOptions(modes[i]);
        original_option.enable_sampling = enable_sampling_options[j];
        original_option.enable_systrace = enable_systrace_options[k];
        TraceOptions new_options;
        EXPECT_TRUE(new_options.SetFromString(original_option.ToString()));
        EXPECT_EQ(original_option.record_mode, new_options.record_mode);
        EXPECT_EQ(original_option.enable_sampling, new_options.enable_sampling);
        EXPECT_EQ(original_option.enable_systrace, new_options.enable_systrace);
      }
    }
  }
}

}  // namespace trace_event
}  // namespace base
