// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {

namespace {

// TODO(lukasza): Support testing on non-Aura platforms (i.e. Android + Mac?).
//
// Notes for the TODO above:
//
// - Why inject/simulate drag-and-drop events at the aura::Window* level.
//
//   - It seems better to inject into UI libraries to cover code *inside* these
//     libraries.  This might complicate simulation a little bit (i.e. picking
//     the right aura::Window and/or aura::client::DragDropDelegate to target),
//     but otherwise important bits of code wouldn't get test coverage (i.e.
//     directly injecting into RenderViewHost->DragTargetDragEnter seems wrong).
//
//   - In theory, we could introduce WebContentsImpl::DragTargetDragEnter (to be
//     used by all UI platforms - so reused by web_contents_view_android.cc,
//     web_contents_view_aura.cc, web_drag_dest_mac.mm), but it feels wrong - UI
//     libraries should already know which widget is the target of the event and
//     so should be able to talk directly to the right widget (i.e. WebContents
//     should not be responsible for mapping coordinates to a widget - this is
//     the job of the UI library).
//
// - Unknowns:
//
//   - Will this work for WebView and Plugin testing.
//
//   - Will this work for simulating dragging from WebContents into outside of
//     the browser (without leaving OS drag&drop machinery in a weird state).

// Test helper for simulating drag and drop happening in WebContents.
class DragAndDropSimulator {
 public:
  explicit DragAndDropSimulator(WebContents* web_contents)
      : web_contents_(web_contents) {}

  // Simulates notification that |text| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(gfx::Point location, const std::string& text) {
    ui::OSExchangeData data;
    data.SetString(base::UTF8ToUTF16(text));
    return SimulateDragEnter(location, data);
  }

  // Simulates dropping of the drag-and-dropped item.
  // SimulateDragEnter needs to be called first.
  // Returns true upon success.
  bool SimulateDrop(gfx::Point location) {
    if (!active_drag_event_) {
      ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
      return false;
    }

    aura::client::DragDropDelegate* delegate = GetDragDropDelegate();
    if (!delegate)
      return false;

    gfx::Point event_location;
    gfx::Point event_root_location;
    CalculateEventLocations(location, &event_location, &event_root_location);
    active_drag_event_->set_location(event_location);
    active_drag_event_->set_root_location(event_root_location);

    delegate->OnDragUpdated(*active_drag_event_);
    delegate->OnPerformDrop(*active_drag_event_);
    return true;
  }

 private:
  bool SimulateDragEnter(gfx::Point location, const ui::OSExchangeData& data) {
    if (active_drag_event_) {
      ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
      return false;
    }

    aura::client::DragDropDelegate* delegate = GetDragDropDelegate();
    if (!delegate)
      return false;

    gfx::Point event_location;
    gfx::Point event_root_location;
    CalculateEventLocations(location, &event_location, &event_root_location);
    active_drag_event_.reset(new ui::DropTargetEvent(
        data, event_location, event_root_location, kDefaultSourceOperations));

    delegate->OnDragEntered(*active_drag_event_);
    delegate->OnDragUpdated(*active_drag_event_);
    return true;
  }

  aura::client::DragDropDelegate* GetDragDropDelegate() {
    gfx::NativeView view = web_contents_->GetContentNativeView();
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(view);
    EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
    return delegate;
  }

  void CalculateEventLocations(gfx::Point web_contents_relative_location,
                               gfx::Point* out_event_location,
                               gfx::Point* out_event_root_location) {
    gfx::NativeView view = web_contents_->GetNativeView();

    *out_event_location = web_contents_relative_location;

    gfx::Point root_location = web_contents_relative_location;
    aura::Window::ConvertPointToTarget(view, view->GetRootWindow(),
                                       &root_location);
    *out_event_location = root_location;
  }

  // These are ui::DropTargetEvent::source_operations_ being sent when manually
  // trying out drag&drop of an image file from Nemo (Ubuntu's file explorer)
  // into a content_shell.
  static constexpr int kDefaultSourceOperations = ui::DragDropTypes::DRAG_MOVE |
                                                  ui::DragDropTypes::DRAG_COPY |
                                                  ui::DragDropTypes::DRAG_LINK;

  WebContents* web_contents_;
  std::unique_ptr<ui::DropTargetEvent> active_drag_event_;

  DISALLOW_COPY_AND_ASSIGN(DragAndDropSimulator);
};

// Helper for waiting for notifications from
// content/test/data/drag_and_drop/event_monitoring.js
class DOMDragEventWaiter {
 public:
  explicit DOMDragEventWaiter(const std::string& event_type_to_wait_for,
                              const ToRenderFrameHost& target)
      : target_frame_name_(target.render_frame_host()->GetFrameName()),
        event_type_to_wait_for_(event_type_to_wait_for),
        dom_message_queue_(
            WebContents::FromRenderFrameHost(target.render_frame_host())) {}

  // Waits until |target| calls reportDragEvent in
  // content/test/data/drag_and_drop/event_monitoring.js with event_type
  // property set to |event_type_to_wait_for|.  (|target| and
  // |event_type_to_wait_for| are passed to the constructor).
  //
  // Returns the event details via |found_event| (in form of a JSON-encoded
  // object).  See content/test/data/drag_and_drop/event_monitoring.js for keys
  // and properties that |found_event| is expected to have.
  //
  // Returns true upon success.  It is okay if |response| is null.
  bool WaitForNextMatchingEvent(std::string* found_event) WARN_UNUSED_RESULT {
    std::string candidate_event;
    bool got_right_event_type = false;
    bool got_right_window_name = false;
    do {
      if (!dom_message_queue_.WaitForMessage(&candidate_event))
        return false;

      got_right_event_type = base::MatchPattern(
          candidate_event, base::StringPrintf("*\"event_type\":\"%s\"*",
                                              event_type_to_wait_for_.c_str()));

      got_right_window_name = base::MatchPattern(
          candidate_event, base::StringPrintf("*\"window_name\":\"%s\"*",
                                              target_frame_name_.c_str()));
    } while (!got_right_event_type || !got_right_window_name);

    if (found_event)
      *found_event = candidate_event;

    return true;
  }

 private:
  std::string target_frame_name_;
  std::string event_type_to_wait_for_;
  DOMMessageQueue dom_message_queue_;
};

const char kTestPagePath[] = "/drag_and_drop/page.html";

}  // namespace

class DragAndDropBrowserTest : public ContentBrowserTest {
 public:
  DragAndDropBrowserTest(){};

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::SetupCrossSiteRedirector(embedded_test_server());
    drag_simulator_.reset(new DragAndDropSimulator(shell()->web_contents()));
  }

  // Navigates to content/test/data/drag_and_drop/page.html, with page origin
  // and frame contents being controlled by the parameters.
  bool NavigateToTestPage(const std::string& main_origin,
                          const std::string& left_frame,
                          const std::string& right_frame) {
    GURL base_url = embedded_test_server()->GetURL(main_origin, kTestPagePath);

    std::string left_arg;
    if (!left_frame.empty()) {
      left_arg = net::EscapeQueryParamValue(
          std::string("/cross-site/") + left_frame, true);
    }
    std::string right_arg;
    if (!right_frame.empty()) {
      right_arg = net::EscapeQueryParamValue(
          std::string("/cross-site/") + right_frame, true);
    }
    std::string query = base::StringPrintf("left=%s&right=%s", left_arg.c_str(),
                                           right_arg.c_str());
    GURL::Replacements query_replacement;
    query_replacement.SetQueryStr(query);
    GURL target_url = base_url.ReplaceComponents(query_replacement);

    return NavigateToURL(shell(), target_url);
  }

  bool SimulateDragEnterToRightFrame(const std::string& text) {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDragEnter(kMiddleOfRightFrame, text);
  }

  bool SimulateDropInRightFrame() {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDrop(kMiddleOfRightFrame);
  }

  RenderFrameHost* right_frame() {
    AssertTestPageIsLoaded();
    return GetFrameByName("right");
  }

 private:
  RenderFrameHost* GetFrameByName(const std::string& name_to_find) {
    WebContentsImpl* web_contents =
            static_cast<WebContentsImpl*>(shell()->web_contents());
    FrameTree* frame_tree = web_contents->GetFrameTree();
    return frame_tree->FindByName(name_to_find)->current_frame_host();
  }

  void AssertTestPageIsLoaded() {
    ASSERT_EQ(kTestPagePath,
              shell()->web_contents()->GetLastCommittedURL().path());
  }

  std::unique_ptr<DragAndDropSimulator> drag_simulator_;

  // Constants with coordinates within content/test/data/drag_and_drop/page.html
  const gfx::Point kMiddleOfLeftFrame = gfx::Point(200, 200);
  const gfx::Point kMiddleOfRightFrame = gfx::Point(400, 200);

  DISALLOW_COPY_AND_ASSIGN(DragAndDropBrowserTest);
};

IN_PROC_BROWSER_TEST_F(DragAndDropBrowserTest, DropTextFromOutside) {
  ASSERT_TRUE(NavigateToTestPage("a.com",
                                 "",  // Left frame is unused by this test.
                                 "c.com/drag_and_drop/drop_target.html"));

  {
    std::string dragover_event;
    DOMDragEventWaiter dragover_waiter("dragover", right_frame());
    ASSERT_TRUE(SimulateDragEnterToRightFrame("Dragged test text"));
    ASSERT_TRUE(dragover_waiter.WaitForNextMatchingEvent(&dragover_event));
    EXPECT_THAT(dragover_event, testing::HasSubstr("\"drop_effect\":\"none\""));
    EXPECT_THAT(dragover_event,
                testing::HasSubstr("\"effect_allowed\":\"all\""));
    EXPECT_THAT(dragover_event,
                testing::HasSubstr("\"mime_types\":\"text/plain\""));
  }

  {
    std::string drop_event;
    DOMDragEventWaiter drop_waiter("drop", right_frame());
    ASSERT_TRUE(SimulateDropInRightFrame());
    ASSERT_TRUE(drop_waiter.WaitForNextMatchingEvent(&drop_event));
    EXPECT_THAT(drop_event, testing::HasSubstr("\"drop_effect\":\"none\""));
    EXPECT_THAT(drop_event, testing::HasSubstr("\"effect_allowed\":\"all\""));
    EXPECT_THAT(drop_event,
                testing::HasSubstr("\"mime_types\":\"text/plain\""));
  }
}

}  // namespace chrome
