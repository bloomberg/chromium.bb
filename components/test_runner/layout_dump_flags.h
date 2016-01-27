// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_
#define COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_

namespace test_runner {

enum class LayoutDumpMode {
  DUMP_AS_TEXT,
  DUMP_AS_MARKUP,
  DUMP_SCROLL_POSITIONS
};

struct LayoutDumpFlags {
  LayoutDumpMode main_dump_mode;

  bool dump_as_printed;
  bool dump_child_frames;
  bool dump_line_box_trees;
  bool debug_render_tree;
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_LAYOUT_DUMP_FLAGS_H_
