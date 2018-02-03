// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_LAYOUT_TEST_RUNTIME_FLAGS_H_
#define CONTENT_SHELL_TEST_RUNNER_LAYOUT_TEST_RUNTIME_FLAGS_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/tracked_dictionary.h"

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

  // If this is non-empty and is_printing is true, pixel dump will be for the
  // named frame printed.
  DEFINE_STRING_LAYOUT_TEST_RUNTIME_FLAG(printing_frame)

  // If true, don't dump output until notifyDone is called.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(wait_until_done)

  // If true, ends the test when a URL is loaded externally via
  // WebFrameClient::loadURLExternally().
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(wait_until_external_url_load)

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

  // Flags influencing behavior of MockContentSettingsClient.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(images_allowed)
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(scripts_allowed)
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(storage_allowed)
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(plugins_allowed)
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(running_insecure_content_allowed)
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(
      dump_web_content_settings_client_callbacks)
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(autoplay_allowed)

  // If true, the test_shell will write a descriptive line for each editing
  // command.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_editting_callbacks)

  // If true, the test_shell will output a descriptive line for each frame
  // load callback.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_frame_load_callbacks)

  // If true, the test_shell will output a descriptive line for each
  // PingLoader dispatched.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_ping_loader_callbacks)

  // If true, the test_shell will output a line of the user gesture status
  // text for some frame load callbacks.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(
      dump_user_gesture_in_frame_load_callbacks)

  // If true, the test_shell will output a descriptive line for each resource
  // load callback.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_resource_load_callbacks)

  // If true, the test_shell will output the MIME type for each resource that
  // was loaded.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_resource_response_mime_types)

  // If true, content_shell will dump the default navigation policy passed to
  // WebFrameClient::decidePolicyForNavigation.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_navigation_policy)

  // If true, output a message when the page title is changed.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_title_changes)

  // If true, the test_shell will print out the icon change notifications.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_icon_changes)

  // If true, the console messages produced by the page will
  // be part of test output.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_console_messages)

  // Desired return value of WebFrameClient::runModalBeforeUnloadDialog.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(
      stay_on_page_after_handling_before_unload)

  // Indicates if the test already tracks a top loading frame (in any of the
  // renderer processes).  This flag is trying to prevent different renderer
  // processes from tracking different top loading frames (i.e. main frame in
  // one renderer and an OOPIF in another renderer).
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(have_top_loading_frame)

  // If true, new windows can be opened via javascript or by plugins. By
  // default, set to false and can be toggled to true using
  // setCanOpenWindows().
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(can_open_windows)

  // If true, output a descriptive line each time WebViewClient::createView
  // is invoked.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_create_view)

  // If true, the test_shell will output descriptive test for spellcheck
  // execution.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_spell_check_callbacks)

  // If true, content_shell will output text for alert(), confirm(), prompt(),
  // etc.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(dump_javascript_dialogs)

  // True if the test called testRunner.setCustomTextOutput.
  DEFINE_BOOL_LAYOUT_TEST_RUNTIME_FLAG(has_custom_text_output)

  // Contains text passed by the test to testRunner.setCustomTextOutput.
  DEFINE_STRING_LAYOUT_TEST_RUNTIME_FLAG(custom_text_output)

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

#endif  // CONTENT_SHELL_TEST_RUNNER_LAYOUT_TEST_RUNTIME_FLAGS_H_
