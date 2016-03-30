// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_H_
#define COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_H_

#include <string>

#include "components/test_runner/layout_test_runtime_flags.h"
#include "components/test_runner/test_runner_export.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace test_runner {

// Dumps textual representation of |frame| contents.  Exact dump mode depends
// on |flags| (i.e. dump_as_text VS dump_as_markup and/or is_printing).
TEST_RUNNER_EXPORT std::string DumpLayout(blink::WebLocalFrame* frame,
                                          const LayoutTestRuntimeFlags& flags);

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_H_
