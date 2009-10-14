// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/debug_util.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StackTrace, OutputToStream) {
  StackTrace trace;

  // Dump the trace into a string.
  std::ostringstream os;
  trace.OutputToStream(&os);
  std::string backtrace_message = os.str();

  size_t frames_found = 0;
  trace.Addresses(&frames_found);
  ASSERT_GE(frames_found, 5u) <<
      "No stack frames found.  Skipping rest of test.";

  // Check if the output has symbol initialization warning.  If it does, fail.
  ASSERT_EQ(backtrace_message.find("Dumping unresolved backtrace"),
            std::string::npos) <<
      "Unable to resolve symbols.  Skipping rest of test.";

#if 0
// TODO(ajwong): Disabling checking of symbol resolution since it depends
//  on whether or not symbols are present, and there are too many
//  configurations to reliably ensure that symbols are findable.
#if defined(OS_MACOSX)

  // Symbol resolution via the backtrace_symbol funciton does not work well
  // in OsX.
  // See this thread:
  //
  //    http://lists.apple.com/archives/darwin-dev/2009/Mar/msg00111.html
  //
  // Just check instead that we find our way back to the "start" symbol
  // which should be the first symbol in the trace.
  //
  // TODO(port): Find a more reliable way to resolve symbols.

  // Expect to at least find main.
  EXPECT_TRUE(backtrace_message.find("start") != std::string::npos)
      << "Expected to find start in backtrace:\n"
      << backtrace_message;

#elif defined(__GLIBCXX__)

  // Expect a demangled symbol.
  EXPECT_TRUE(backtrace_message.find("testing::Test::Run()") !=
              std::string::npos)
      << "Expected a demangled symbol in backtrace:\n"
      << backtrace_message;
#else  // defined(__GLIBCXX__)

  // Expect to at least find main.
  EXPECT_TRUE(backtrace_message.find("main") != std::string::npos)
      << "Expected to find main in backtrace:\n"
      << backtrace_message;

#if defined(OS_WIN)
// MSVC doesn't allow the use of C99's __func__ within C++, so we fake it with
// MSVC's __FUNCTION__ macro.
#define __func__ __FUNCTION__
#endif

  // Expect to find this function as well.
  // Note: This will fail if not linked with -rdynamic (aka -export_dynamic)
  EXPECT_TRUE(backtrace_message.find(__func__) != std::string::npos)
      << "Expected to find " << __func__ << " in backtrace:\n"
      << backtrace_message;

#endif  // define(OS_MACOSX)
#endif
}
