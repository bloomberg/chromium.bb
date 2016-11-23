// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace chrome {

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

// Test helper for simulating drag and drop happening in WebContents.
class DragAndDropSimulator {
 public:
  explicit DragAndDropSimulator(content::WebContents* web_contents)
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
    *out_event_root_location = root_location;
  }

  // These are ui::DropTargetEvent::source_operations_ being sent when manually
  // trying out drag&drop of an image file from Nemo (Ubuntu's file explorer)
  // into a content_shell.
  static constexpr int kDefaultSourceOperations = ui::DragDropTypes::DRAG_MOVE |
                                                  ui::DragDropTypes::DRAG_COPY |
                                                  ui::DragDropTypes::DRAG_LINK;

  content::WebContents* web_contents_;
  std::unique_ptr<ui::DropTargetEvent> active_drag_event_;

  DISALLOW_COPY_AND_ASSIGN(DragAndDropSimulator);
};

// Helper for waiting until a drag-and-drop starts (e.g. in response to a
// mouse-down + mouse-move simulated by the test).
class DragStartWaiter : public aura::client::DragDropClient {
 public:
  // Starts monitoring |web_contents| for a start of a drag-and-drop.
  // While alive, prevents a real, OS-level drag-and-drop from starting
  // for this particular |web_contents|.
  explicit DragStartWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents),
        message_loop_runner_(new content::MessageLoopRunner),
        drag_started_(false) {
    DCHECK(web_contents_);

    // Intercept calls to the old DragDropClient.
    gfx::NativeWindow root_window =
        web_contents_->GetContentNativeView()->GetRootWindow();
    old_client_ = aura::client::GetDragDropClient(root_window);
    aura::client::SetDragDropClient(root_window, this);
  }

  ~DragStartWaiter() override {
    // Restore the original DragDropClient.
    gfx::NativeWindow root_window =
        web_contents_->GetContentNativeView()->GetRootWindow();
    aura::client::SetDragDropClient(root_window, old_client_);
  }

  // Waits until we almost report a drag-and-drop start to the OS
  // (notifying the OS will be prevented to help the test continue
  // without entering a nested message loop).
  //
  // Returns true if drag and drop has indeed started (and in this
  // case populates |text|, |html| and other parameters with data
  // that would have been passed to the OS).
  void WaitUntilDragStartIsIntercepted(
      std::string* text,
      std::string* html,
      int* operation,
      gfx::Point* location_inside_web_contents) {
    message_loop_runner_->Run();

    // message_loop_runner_->Quit is only called from StartDragAndDrop.
    DCHECK(drag_started_);

    *text = text_;
    *html = html_;
    *operation = operation_;
    *location_inside_web_contents = location_inside_web_contents_;
  }

  // aura::client::DragDropClient overrides:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override {
    DCHECK(!drag_started_);
    if (!drag_started_) {
      drag_started_ = true;
      message_loop_runner_->Quit();

      base::string16 text;
      if (data.GetString(&text))
        text_ = base::UTF16ToUTF8(text);
      else
        text_ = "<no text>";

      GURL base_url;
      base::string16 html;
      if (data.GetHtml(&html, &base_url))
        html_ = base::UTF16ToUTF8(html);
      else
        html_ = "<no html>";

      gfx::Rect bounds =
          web_contents_->GetContentNativeView()->GetBoundsInScreen();
      location_inside_web_contents_ =
          screen_location - gfx::Vector2d(bounds.x(), bounds.y());

      operation_ = operation;
    }

    // Forwarding to |old_client_| is undesirable, because test cannot control
    // next steps after a nested drag-and-drop loop is entered at the OS level
    // (as is the case in Windows, via DoDragDrop).  Instead, in the test we
    // kind of ignore renderer's request to start drag and drop and never
    // forward this request to the OS-specific layers.
    return 0;
  }

  void DragCancel() override {
    ADD_FAILURE() << "Unexpected call to DragCancel";
  }

  bool IsDragDropInProgress() override { return drag_started_; }

 private:
  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  aura::client::DragDropClient* old_client_;

  // Data captured during the first intercepted StartDragAndDrop call.
  bool drag_started_;
  std::string text_;
  std::string html_;
  int operation_;
  gfx::Point location_inside_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(DragStartWaiter);
};

// Helper for waiting for notifications from
// content/test/data/drag_and_drop/event_monitoring.js
class DOMDragEventWaiter {
 public:
  explicit DOMDragEventWaiter(const std::string& event_type_to_wait_for,
                              const content::ToRenderFrameHost& target)
      : target_frame_name_(target.render_frame_host()->GetFrameName()),
        event_type_to_wait_for_(event_type_to_wait_for),
        dom_message_queue_(content::WebContents::FromRenderFrameHost(
            target.render_frame_host())) {}

  // Waits until |target| calls reportDragEvent in
  // chrome/test/data/drag_and_drop/event_monitoring.js with event_type
  // property set to |event_type_to_wait_for|.  (|target| and
  // |event_type_to_wait_for| are passed to the constructor).
  //
  // Returns the event details via |found_event| (in form of a JSON-encoded
  // object).  See chrome/test/data/drag_and_drop/event_monitoring.js for keys
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
  content::DOMMessageQueue dom_message_queue_;

  DISALLOW_COPY_AND_ASSIGN(DOMDragEventWaiter);
};

// Helper for verifying contents of DOM events associated with drag-and-drop.
class DOMDragEventVerifier {
 public:
  DOMDragEventVerifier() {}

  void set_expected_client_position(const std::string& value) {
    expected_client_position_ = value;
  }

  void set_expected_drop_effect(const std::string& value) {
    expected_drop_effect_ = value;
  }

  void set_expected_effect_allowed(const std::string& value) {
    expected_effect_allowed_ = value;
  }

  void set_expected_mime_types(const std::string& value) {
    expected_mime_types_ = value;
  }

  void set_expected_page_position(const std::string& value) {
    expected_page_position_ = value;
  }

  // Returns a matcher that will match a std::string (drag event data - e.g.
  // one returned by DOMDragEventWaiter::WaitForNextMatchingEvent) if it matches
  // the expectations of this DOMDragEventVerifier.
  testing::Matcher<std::string> Matches() const {
    return testing::AllOf(
        FieldMatches("client_position", expected_client_position_),
        FieldMatches("drop_effect", expected_drop_effect_),
        FieldMatches("effect_allowed", expected_effect_allowed_),
        FieldMatches("mime_types", expected_mime_types_),
        FieldMatches("page_position", expected_page_position_));
  }

 private:
  static testing::Matcher<std::string> FieldMatches(
      const std::string& field_name,
      const std::string& expected_value) {
    if (expected_value == "<no expectation>")
      return testing::A<std::string>();

    return testing::HasSubstr(base::StringPrintf(
        "\"%s\":\"%s\"", field_name.c_str(), expected_value.c_str()));
  }

  std::string expected_drop_effect_ = "<no expectation>";
  std::string expected_effect_allowed_ = "<no expectation>";
  std::string expected_mime_types_ = "<no expectation>";
  std::string expected_client_position_ = "<no expectation>";
  std::string expected_page_position_ = "<no expectation>";

  DISALLOW_COPY_AND_ASSIGN(DOMDragEventVerifier);
};

const char kTestPagePath[] = "/drag_and_drop/page.html";

}  // namespace

class DragAndDropBrowserTest : public InProcessBrowserTest,
                               public testing::WithParamInterface<bool> {
 public:
  DragAndDropBrowserTest(){};

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    drag_simulator_.reset(new DragAndDropSimulator(web_contents()));
  }

  bool use_cross_site_subframe() {
    // This is controlled by gtest's test param from INSTANTIATE_TEST_CASE_P.
    return GetParam();
  }

  content::RenderFrameHost* left_frame() {
    AssertTestPageIsLoaded();
    return GetFrameByName("left");
  }

  content::RenderFrameHost* right_frame() {
    AssertTestPageIsLoaded();
    return GetFrameByName("right");
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  //////////////////////
  // Navigation helpers.

  bool NavigateToTestPage(const std::string& origin_of_main_frame) {
    GURL url =
        embedded_test_server()->GetURL(origin_of_main_frame, kTestPagePath);
    ui_test_utils::NavigateToURL(browser(), url);
    return web_contents()->GetLastCommittedURL() == url;
  }

  // Navigates the left frame to |filename| (found under
  // chrome/test/data/drag_and_drop directory).
  bool NavigateLeftFrame(const std::string& origin,
                         const std::string& filename) {
    AssertTestPageIsLoaded();
    return NavigateNamedFrame("left", origin, filename);
  }

  // Navigates the right frame to |filename| (found under
  // chrome/test/data/drag_and_drop directory).
  bool NavigateRightFrame(const std::string& origin,
                          const std::string& filename) {
    AssertTestPageIsLoaded();
    return NavigateNamedFrame("right", origin, filename);
  }

  ////////////////////////////////////////////////////////////
  // Simulation of starting a drag-and-drop (using the mouse).

  bool SimulateMouseDownAndDragStartInLeftFrame() {
    AssertTestPageIsLoaded();
    if (!SimulateMouseMove(kMiddleOfLeftFrame) || !SimulateMouseDown() ||
        !SimulateMouseMove(expected_location_of_drag_start_in_left_frame()))
      return false;

    return true;
  }

  gfx::Point expected_location_of_drag_start_in_left_frame() {
    // TODO(crbug.com/653490): The delta below should exceed kDragThresholdX and
    // kDragThresholdY from MouseEventManager.cpp in blink.  Ideally, it would
    // come from the OS instead.
    return kMiddleOfLeftFrame + gfx::Vector2d(10, 10);
  }

  bool SimulateMouseUp() {
    return ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                              ui_controls::UP);
  }

  ////////////////////////////////////////////////////////////////////
  // Simulation of dragging from outside the browser into web contents
  // (using DragAndDropSimulator, not simulating mouse events).

  bool SimulateDragEnterToRightFrame(const std::string& text) {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDragEnter(kMiddleOfRightFrame, text);
  }

  bool SimulateDropInRightFrame() {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDrop(kMiddleOfRightFrame);
  }

 private:
  bool SimulateMouseDown() {
    return ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                              ui_controls::DOWN);
  }

  bool SimulateMouseMove(const gfx::Point& location_inside_web_contents) {
    gfx::Rect bounds = web_contents()->GetContainerBounds();
    return ui_test_utils::SendMouseMoveSync(
        gfx::Point(bounds.x() + location_inside_web_contents.x(),
                   bounds.y() + location_inside_web_contents.y()));
  }

  bool NavigateNamedFrame(const std::string& frame_name,
                          const std::string& origin,
                          const std::string& filename) {
    content::RenderFrameHost* frame = GetFrameByName(frame_name);
    if (!frame)
      return false;

    std::string script;
    int response = 0;

    // Navigate the frame and wait for the load event.
    script = base::StringPrintf(
        "location.href = '/cross-site/%s/drag_and_drop/%s';\n"
        "setTimeout(function() { domAutomationController.send(42); }, 0);",
        origin.c_str(), filename.c_str());
    content::TestFrameNavigationObserver observer(frame);
    if (!content::ExecuteScriptAndExtractInt(frame, script, &response))
      return false;
    if (response != 42)
      return false;
    observer.Wait();

    // |frame| might have been swapped-out during a cross-site navigation,
    // therefore we need to get the current RenderFrameHost to work against
    // going forward.
    frame = GetFrameByName(frame_name);
    DCHECK(frame);

    // Wait until frame contents (e.g. images) have painted (which should happen
    // in the animation frame that *starts* after the onload event - therefore
    // we need to wait for 2 animation frames).
    script = std::string(
        "requestAnimationFrame(function() {\n"
        "  requestAnimationFrame(function() {\n"
        "    domAutomationController.send(43);\n"
        "  });\n"
        "});\n");
    if (!content::ExecuteScriptAndExtractInt(frame, script, &response))
      return false;
    if (response != 43)
      return false;

    return true;
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name_to_find) {
    content::RenderFrameHost* result = nullptr;
    for (content::RenderFrameHost* rfh : web_contents()->GetAllFrames()) {
      if (rfh->GetFrameName() == name_to_find) {
        if (result) {
          ADD_FAILURE() << "More than one frame named "
                        << "'" << name_to_find << "'";
          return nullptr;
        }
        result = rfh;
      }
    }

    EXPECT_TRUE(result) << "Couldn't find a frame named "
                        << "'" << name_to_find << "'";
    return result;
  }

  void AssertTestPageIsLoaded() {
    ASSERT_EQ(kTestPagePath, web_contents()->GetLastCommittedURL().path());
  }

  std::unique_ptr<DragAndDropSimulator> drag_simulator_;

  // Constants with coordinates within content/test/data/drag_and_drop/page.html
  const gfx::Point kMiddleOfLeftFrame = gfx::Point(200, 200);
  const gfx::Point kMiddleOfRightFrame = gfx::Point(400, 200);

  DISALLOW_COPY_AND_ASSIGN(DragAndDropBrowserTest);
};

IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DropTextFromOutside) {
  std::string frame_site = use_cross_site_subframe() ? "b.com" : "a.com";
  ASSERT_TRUE(NavigateToTestPage("a.com"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  // Setup test expectations.
  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(100, 100)");
  expected_dom_event_data.set_expected_drop_effect("none");
  expected_dom_event_data.set_expected_effect_allowed("all");
  expected_dom_event_data.set_expected_mime_types("text/plain");
  expected_dom_event_data.set_expected_page_position("(100, 100)");

  // Drag text from outside the browser into/over the right frame.
  {
    DOMDragEventWaiter dragover_waiter("dragover", right_frame());
    ASSERT_TRUE(SimulateDragEnterToRightFrame("Dragged test text"));

    std::string dragover_event;
    ASSERT_TRUE(dragover_waiter.WaitForNextMatchingEvent(&dragover_event));
    EXPECT_THAT(dragover_event, expected_dom_event_data.Matches());
  }

  // Drop into the right frame.
  {
    DOMDragEventWaiter drop_waiter("drop", right_frame());
    ASSERT_TRUE(SimulateDropInRightFrame());

    std::string drop_event;
    ASSERT_TRUE(drop_waiter.WaitForNextMatchingEvent(&drop_event));
    EXPECT_THAT(drop_event, expected_dom_event_data.Matches());
  }
}

IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DragStartInFrame) {
  std::string frame_site = use_cross_site_subframe() ? "b.com" : "a.com";
  ASSERT_TRUE(NavigateToTestPage("a.com"));
  ASSERT_TRUE(NavigateLeftFrame(frame_site, "image_source.html"));

  // Setup test expectations.
  // (dragstart event handler in image_source.html is asking for "copy" only).
  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(100, 100)");
  expected_dom_event_data.set_expected_drop_effect("none");
  expected_dom_event_data.set_expected_effect_allowed("copy");
  expected_dom_event_data.set_expected_mime_types(
      "Files,text/html,text/uri-list");
  expected_dom_event_data.set_expected_page_position("(100, 100)");

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  DOMDragEventWaiter dragstart_event_waiter("dragstart", left_frame());
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // Verify Javascript event data.
  {
    std::string dragstart_event;
    EXPECT_TRUE(
        dragstart_event_waiter.WaitForNextMatchingEvent(&dragstart_event));
    EXPECT_THAT(dragstart_event, expected_dom_event_data.Matches());
  }

  // Verify data being passed to the OS.
  {
    std::string text;
    std::string html;
    int operation = 0;
    gfx::Point location_inside_web_contents;
    drag_start_waiter.WaitUntilDragStartIsIntercepted(
        &text, &html, &operation, &location_inside_web_contents);
    EXPECT_EQ(embedded_test_server()->GetURL(frame_site,
                                             "/image_decoding/droids.jpg"),
              text);
    EXPECT_THAT(html,
                testing::MatchesRegex("<img .*src=\""
                                      "http://.*/image_decoding/droids.jpg"
                                      "\">"));
    EXPECT_EQ(expected_location_of_drag_start_in_left_frame(),
              location_inside_web_contents);
    EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, operation);
  }

  // Try to leave everything in a clean state.
  SimulateMouseUp();
}

INSTANTIATE_TEST_CASE_P(
    SameSiteSubframe, DragAndDropBrowserTest, ::testing::Values(false));

INSTANTIATE_TEST_CASE_P(
    CrossSiteSubframe, DragAndDropBrowserTest, ::testing::Values(true));

}  // namespace chrome
