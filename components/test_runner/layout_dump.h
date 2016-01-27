// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_H_
#define COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_H_

#include <string>

#include "components/test_runner/layout_dump_flags.h"
#include "components/test_runner/test_runner_export.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace test_runner {

TEST_RUNNER_EXPORT std::string DumpLayout(blink::WebLocalFrame* frame,
                                          const LayoutDumpFlags& flags);

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_H_
