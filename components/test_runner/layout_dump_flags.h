// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_
#define COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/test_runner/test_runner_export.h"
#include "components/test_runner/tracked_dictionary.h"

namespace test_runner {

// TODO(lukasza): Rename to LayoutTestRuntimeFlags.
class TEST_RUNNER_EXPORT LayoutDumpFlags {
 public:
  // Creates default flags (see also the Reset method).
  LayoutDumpFlags();

  // Resets all the values to their defaults.
  void Reset();

  TrackedDictionary& tracked_dictionary() { return dict_; }
  const TrackedDictionary& tracked_dictionary() const { return dict_; }

#define DEFINE_BOOL_LAYOUT_DUMP_FLAG(name)                          \
  bool name() const {                                               \
    bool result;                                                    \
    bool found = dict_.current_values().GetBoolean(#name, &result); \
    DCHECK(found);                                                  \
    return result;                                                  \
  }                                                                 \
  void set_##name(bool new_value) { dict_.SetBoolean(#name, new_value); }

  // If true, the test_shell will generate pixel results in DumpAsText mode.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(generate_pixel_results)

  // If true, the test_shell will produce a plain text dump rather than a
  // text representation of the renderer.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(dump_as_text)

  // If true and if dump_as_text_ is true, the test_shell will recursively
  // dump all frames as plain text.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(dump_child_frames_as_text)

  // If true, the test_shell will produce a dump of the DOM rather than a text
  // representation of the renderer.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(dump_as_markup)

  // If true and if dump_as_markup_ is true, the test_shell will recursively
  // produce a dump of the DOM rather than a text representation of the
  // renderer.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(dump_child_frames_as_markup)

  // If true, the test_shell will print out the child frame scroll offsets as
  // well.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(dump_child_frame_scroll_positions)

  // If true, layout is to target printed pages.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(is_printing)

  // If true, don't dump output until notifyDone is called.
  DEFINE_BOOL_LAYOUT_DUMP_FLAG(wait_until_done)

#undef DEFINE_BOOL_LAYOUT_DUMP_FLAG

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

  DISALLOW_COPY_AND_ASSIGN(LayoutDumpFlags);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_
