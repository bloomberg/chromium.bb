// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/layout_dump_flags.h"

namespace test_runner {

LayoutDumpFlags::LayoutDumpFlags() {
  Reset();
}

void LayoutDumpFlags::Reset() {
  set_generate_pixel_results(true);

  set_dump_as_text(false);
  set_dump_child_frames_as_text(false);

  set_dump_as_markup(false);
  set_dump_child_frames_as_markup(false);

  set_dump_child_frame_scroll_positions(false);

  set_is_printing(false);

  set_wait_until_done(false);

  // No need to report the initial state - only the future delta is important.
  tracked_dictionary().ResetChangeTracking();
}

}  // namespace test_runner
