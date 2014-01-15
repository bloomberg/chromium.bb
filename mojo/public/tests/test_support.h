// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_
#define MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_

#include <string>

#include "mojo/public/system/core_cpp.h"

namespace mojo {
namespace test {

bool WriteTextMessage(MessagePipeHandle handle, const std::string& text);
bool ReadTextMessage(MessagePipeHandle handle, std::string* text);

// Run |single_iteration| an appropriate number of times and report its
// performance appropriately. (This actually runs |single_iteration| for a fixed
// amount of time and reports the number of iterations per unit time.)
typedef void (*PerfTestSingleIteration)(void* closure);
void IterateAndReportPerf(const char* test_name,
                          PerfTestSingleIteration single_iteration,
                          void* closure);

MojoResult WriteEmptyMessage(const MessagePipeHandle& handle);
MojoResult ReadEmptyMessage(const MessagePipeHandle& handle);

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_
