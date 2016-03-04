// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_
#define COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_

namespace test_runner {

struct LayoutDumpFlags {
  LayoutDumpFlags(bool dump_as_text,
                  bool dump_child_frames_as_text,
                  bool dump_as_markup,
                  bool dump_child_frames_as_markup,
                  bool dump_child_frame_scroll_positions,
                  bool is_printing)
      : dump_as_text(dump_as_text),
        dump_child_frames_as_text(dump_child_frames_as_text),
        dump_as_markup(dump_as_text),
        dump_child_frames_as_markup(dump_child_frames_as_markup),
        dump_child_frame_scroll_positions(dump_child_frame_scroll_positions),
        is_printing(is_printing) {}

  // Default constructor needed for IPC.
  //
  // Default constructor is |= default| to make sure LayoutDumpFlags is a POD
  // (required until we can remove content/shell/browser dependency on it).
  LayoutDumpFlags() = default;

  // If true, the test_shell will produce a plain text dump rather than a
  // text representation of the renderer.
  bool dump_as_text;

  // If true and if dump_as_text_ is true, the test_shell will recursively
  // dump all frames as plain text.
  bool dump_child_frames_as_text;

  // If true, the test_shell will produce a dump of the DOM rather than a text
  // representation of the layout objects.
  bool dump_as_markup;

  // If true and if dump_as_markup_ is true, the test_shell will recursively
  // produce a dump of the DOM rather than a text representation of the
  // layout objects.
  bool dump_child_frames_as_markup;

  // If true, the test_shell will print out the child frame scroll offsets as
  // well.
  bool dump_child_frame_scroll_positions;

  // Reports whether recursing over child frames is necessary.
  bool dump_child_frames() const {
    if (dump_as_text)
      return dump_child_frames_as_text;
    else if (dump_as_markup)
      return dump_child_frames_as_markup;
    else
      return dump_child_frame_scroll_positions;
  }

  // If true, layout is to target printed pages.
  bool is_printing;
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_
