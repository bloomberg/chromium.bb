// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_LAYOUT_TEST_RUNTIME_FLAGS_H_
#define COMPONENTS_TEST_RUNNER_LAYOUT_TEST_RUNTIME_FLAGS_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/test_runner/test_runner_export.h"
#include "components/test_runner/tracked_dictionary.h"

namespace test_runner {

// LayoutTestRuntimeFlags stores flags controlled by layout tests at runtime
// (i.e. by calling testRunner.dumpAsText() or testRunner.waitUntilDone()).
// Changes to the flags are tracked (to help replicate them across renderers).
class TEST_RUNNER_EXPORT LayoutTestRuntimeFlags {
 public:
  // Creates default flags (see also the Reset method).
  LayoutTestRuntimeFlags();

  // Resets all the values to their defaults.
  void Reset();

  TrackedDictionary& tracked_dictionary() { return dict_; }

#define DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(name)                  \
  bool name() const {                                               \
    bool result;                                                    \
    bool found = dict_.current_values().GetBoolean(#name, &result); \
    DCHECK(found);                                                  \
    return result;                                                  \
  }                                                                 \
  void set_##name(bool new_value) { dict_.SetBoolean(#name, new_value); }

#define DEFINE_STRING_LAYOUT_TEST_RUNTIME_FLAG(name)               \
  std::string name() const {                                       \
    std::string result;                                            \
    bool found = dict_.current_values().GetString(#name, &result); \
    DCHECK(found);                                                 \
    return result;                                                 \
  }                                                                \
  void set_##name(const std::string& new_value) {                  \
    dict_.SetString(#name, new_value);                             \
  }

  // If true, the test_shell will generate pixel results in DumpAsText mode.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(generate_pixel_results)

  // If true, the test_shell will produce a plain text dump rather than a
  // text representation of the renderer.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_as_text)

  // If true and if dump_as_text_ is true, the test_shell will recursively
  // dump all frames as plain text.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_child_frames_as_text)

  // If true, the test_shell will produce a dump of the DOM rather than a text
  // representation of the renderer.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_as_markup)

  // If true and if dump_as_markup_ is true, the test_shell will recursively
  // produce a dump of the DOM rather than a text representation of the
  // renderer.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_child_frames_as_markup)

  // If true, the test_shell will print out the child frame scroll offsets as
  // well.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_child_frame_scroll_positions)

  // If true, layout is to target printed pages.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(is_printing)

  // If true, don't dump output until notifyDone is called.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(wait_until_done)

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(policy_delegate_enabled)

  // Toggles the behavior of the policy delegate. If true, then navigations
  // will be allowed. Otherwise, they will be ignored (dropped).
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(policy_delegate_is_permissive)

  // If true, the policy delegate will signal layout test completion.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(policy_delegate_should_notify_done)

  // If true, the test_shell will draw the bounds of the current selection rect
  // taking possible transforms of the selection rect into account.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_selection_rect)

  // If true, the test_shell will dump the drag image as pixel results.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_drag_image)

  // Contents of Accept-Language HTTP header requested by the test.
  DEFINE_STRING_LAYOUT_TEST_RUNTIME_FLAG(accept_languages)

#undef DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG
#undef DEFINE_STRING_LAYOUT_TEST_RUNTIME_FLAG

  // Reports whether recursing over child frames is necessary.
  bool dump_child_frames() const {
    if (dump_as_text())
      return dump_child_frames_as_text();
    else if (dump_as_markup())
      return dump_child_frames_as_markup();
    else
      return dump_child_frame_scroll_positions();
  }

 private:
  TrackedDictionary dict_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestRuntimeFlags);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_LAYOUT_TEST_RUNTIME_FLAGS_H_
