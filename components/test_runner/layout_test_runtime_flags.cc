// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/layout_test_runtime_flags.h"

namespace test_runner {

LayoutTestRuntimeFlags::LayoutTestRuntimeFlags() {
  Reset();
}

void LayoutTestRuntimeFlags::Reset() {
  set_generate_pixel_results(true);

  set_dump_as_text(false);
  set_dump_child_frames_as_text(false);

  set_dump_as_markup(false);
  set_dump_child_frames_as_markup(false);

  set_dump_child_frame_scroll_positions(false);

  set_is_printing(false);

  set_policy_delegate_enabled(false);
  set_policy_delegate_is_permissive(false);
  set_policy_delegate_should_notify_done(false);
  set_wait_until_done(false);
  set_wait_until_external_url_load(false);

  set_dump_selection_rect(false);
  set_dump_drag_image(false);

  set_accept_languages("");

  set_dump_web_content_settings_client_callbacks(false);
  set_images_allowed(true);
  set_media_allowed(true);
  set_scripts_allowed(true);
  set_storage_allowed(true);
  set_plugins_allowed(true);
  set_displaying_insecure_content_allowed(false);
  set_running_insecure_content_allowed(false);

  set_dump_editting_callbacks(false);
  set_dump_frame_load_callbacks(false);
  set_dump_ping_loader_callbacks(false);
  set_dump_user_gesture_in_frame_load_callbacks(false);
  set_dump_resource_load_callbacks(false);
  set_dump_resource_priorities(false);
  set_dump_resource_response_mime_types(false);
  set_dump_navigation_policy(false);
  set_intercept_post_message(false);

  set_dump_title_changes(false);
  set_dump_icon_changes(false);

  set_stay_on_page_after_handling_before_unload(false);

  // No need to report the initial state - only the future delta is important.
  tracked_dictionary().ResetChangeTracking();
}

}  // namespace test_runner
