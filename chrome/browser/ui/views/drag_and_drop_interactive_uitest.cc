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
  explicit DragStartWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents),
        message_loop_runner_(new content::MessageLoopRunner),
        suppress_passing_of_start_drag_further_(false),
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

  // Waits until we almost report a drag-and-drop start to the OS.
  // At that point
  // 1) the callback from PostTaskWhenDragStarts will be posted.
  // 2) the drag-start request will be forwarded to the OS
  //    (unless SuppressPassingStartDragFurther method was called).
  //
  // Note that if SuppressPassingStartDragFurther was not called then
  // WaitUntilDragStart can take a long time to return (it returns only after
  // the OS decides that the drag-and-drop has ended).
  //
  // Before returning populates |text|, |html| and other parameters with data
  // that would have been passed to the OS).  If the caller is not interested in
  // this data, then the corresponding argument can be null.
  void WaitUntilDragStart(std::string* text,
                          std::string* html,
                          int* operation,
                          gfx::Point* location_inside_web_contents) {
    message_loop_runner_->Run();

    // message_loop_runner_->Quit is only called from StartDragAndDrop.
    DCHECK(drag_started_);

    if (text)
      *text = text_;
    if (html)
      *html = html_;
    if (operation)
      *operation = operation_;
    if (location_inside_web_contents)
      *location_inside_web_contents = location_inside_web_contents_;
  }

  void SuppressPassingStartDragFurther() {
    suppress_passing_of_start_drag_further_ = true;
  }

  void PostTaskWhenDragStarts(const base::Closure& callback) {
    callback_to_run_inside_drag_and_drop_message_loop_ = callback;
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

    if (!callback_to_run_inside_drag_and_drop_message_loop_.is_null()) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          std::move(callback_to_run_inside_drag_and_drop_message_loop_));
      callback_to_run_inside_drag_and_drop_message_loop_.Reset();
    }

    if (suppress_passing_of_start_drag_further_)
      return 0;

    // Start a nested drag-and-drop loop (might not return for a long time).
    return old_client_->StartDragAndDrop(data, root_window, source_window,
                                         screen_location, operation, source);
  }

  void DragCancel() override {
    ADD_FAILURE() << "Unexpected call to DragCancel";
  }

  bool IsDragDropInProgress() override { return drag_started_; }

 private:
  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  aura::client::DragDropClient* old_client_;
  base::Closure callback_to_run_inside_drag_and_drop_message_loop_;
  bool suppress_passing_of_start_drag_further_;

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

  struct DragImageBetweenFrames_TestState;
  void DragImageBetweenFrames_Step2(DragImageBetweenFrames_TestState*);
  void DragImageBetweenFrames_Step3(DragImageBetweenFrames_TestState*);

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

  bool SimulateMouseMoveToLeftFrame() {
    AssertTestPageIsLoaded();
    return SimulateMouseMove(kMiddleOfLeftFrame);
  }

  bool SimulateMouseMoveToRightFrame() {
    AssertTestPageIsLoaded();
    return SimulateMouseMove(kMiddleOfRightFrame);
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
  // Constants with coordinates within content/test/data/drag_and_drop/page.html
  // The precise frame center is at 200,200 and 400,200 coordinates, but slight
  // differences between left and right frame hopefully make it easier to detect
  // incorrect dom_drag_and_drop_event.clientX/Y values in test asserts.
  const gfx::Point kMiddleOfLeftFrame = gfx::Point(155, 150);
  const gfx::Point kMiddleOfRightFrame = gfx::Point(455, 250);

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

  DISALLOW_COPY_AND_ASSIGN(DragAndDropBrowserTest);
};

// Scenario: drag text from outside the browser and drop to the right frame.
// Test coverage: dragover, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DropTextFromOutside) {
  std::string frame_site = use_cross_site_subframe() ? "b.com" : "a.com";
  ASSERT_TRUE(NavigateToTestPage("a.com"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  // Setup test expectations.
  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(155, 150)");
  expected_dom_event_data.set_expected_drop_effect("none");
  expected_dom_event_data.set_expected_effect_allowed("all");
  expected_dom_event_data.set_expected_mime_types("text/plain");
  expected_dom_event_data.set_expected_page_position("(155, 150)");

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

// Scenario: starting a drag in left frame
// Test coverage: dragstart DOM event, dragstart data passed to the OS.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DragStartInFrame) {
  std::string frame_site = use_cross_site_subframe() ? "b.com" : "a.com";
  ASSERT_TRUE(NavigateToTestPage("a.com"));
  ASSERT_TRUE(NavigateLeftFrame(frame_site, "image_source.html"));

  // Setup test expectations.
  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(55, 50)");
  expected_dom_event_data.set_expected_drop_effect("none");
  // (dragstart event handler in image_source.html is asking for "copy" only).
  expected_dom_event_data.set_expected_effect_allowed("copy");
  expected_dom_event_data.set_expected_page_position("(55, 50)");

  // TODO(lukasza): Figure out why the dragstart event
  // - lists "Files" on the mime types list,
  // - doesn't list "text/plain" on the mime types list.
  // (i.e. why expectations below differ from expectations for dragenter,
  // dragover, dragend and/or drop events in DragImageBetweenFrames test).
  expected_dom_event_data.set_expected_mime_types(
      "Files,text/html,text/uri-list");

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.SuppressPassingStartDragFurther();
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
    drag_start_waiter.WaitUntilDragStart(&text, &html, &operation,
                                         &location_inside_web_contents);
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

// There is no known way to execute test-controlled tasks during
// a drag-and-drop loop run by Windows OS.
#if defined(OS_WIN)
#define MAYBE_DragImageBetweenFrames DISABLED_DragImageBetweenFrames
#else
#define MAYBE_DragImageBetweenFrames DragImageBetweenFrames
#endif

// Data that needs to be shared across multiple test steps below
// (i.e. across DragImageBetweenFrames_Step2 and DragImageBetweenFrames_Step3).
struct DragAndDropBrowserTest::DragImageBetweenFrames_TestState {
  DOMDragEventVerifier expected_dom_event_data;
  std::unique_ptr<DOMDragEventWaiter> dragstart_event_waiter;
  std::unique_ptr<DOMDragEventWaiter> drop_event_waiter;
  std::unique_ptr<DOMDragEventWaiter> dragend_event_waiter;
};

// Scenario: drag an image from the left into the right frame.
// Test coverage: dragleave, dragenter, dragover, dragend, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, MAYBE_DragImageBetweenFrames) {
  // Note that drag and drop will not expose data across cross-site frames on
  // the same page - this is why the same |frame_site| is used below both for
  // the left and the right frame.  See also https://crbug.com/59081.
  std::string frame_site = use_cross_site_subframe() ? "b.com" : "a.com";
  ASSERT_TRUE(NavigateToTestPage("a.com"));
  ASSERT_TRUE(NavigateLeftFrame(frame_site, "image_source.html"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  // Setup test expectations.
  DragAndDropBrowserTest::DragImageBetweenFrames_TestState state;
  state.expected_dom_event_data.set_expected_client_position("(55, 50)");
  state.expected_dom_event_data.set_expected_drop_effect("none");
  // (dragstart event handler in image_source.html is asking for "copy" only).
  state.expected_dom_event_data.set_expected_effect_allowed("copy");
  state.expected_dom_event_data.set_expected_mime_types(
      "text/html,text/plain,text/uri-list");
  state.expected_dom_event_data.set_expected_page_position("(55, 50)");

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.PostTaskWhenDragStarts(
      base::Bind(&DragAndDropBrowserTest::DragImageBetweenFrames_Step2,
                 base::Unretained(this), base::Unretained(&state)));
  state.dragstart_event_waiter.reset(
      new DOMDragEventWaiter("dragstart", left_frame()));
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // The next step of the test (DragImageBetweenFrames_Step2) runs inside the
  // nested drag-and-drop message loop - the call below won't return until the
  // drag-and-drop has already ended.
  drag_start_waiter.WaitUntilDragStart(nullptr, nullptr, nullptr, nullptr);

  DragImageBetweenFrames_Step3(&state);
}

void DragAndDropBrowserTest::DragImageBetweenFrames_Step2(
    DragAndDropBrowserTest::DragImageBetweenFrames_TestState* state) {
  // Verify dragstart DOM event.
  {
    std::string dragstart_event;
    EXPECT_TRUE(state->dragstart_event_waiter->WaitForNextMatchingEvent(
        &dragstart_event));
    state->dragstart_event_waiter.reset();
  }

  // While dragging, move mouse within the left frame.
  // Without this extra mouse move we wouldn't get a dragleave event later on.
  ASSERT_TRUE(SimulateMouseMoveToLeftFrame());

  // While dragging, move mouse from the left into the right frame.
  // This should trigger dragleave and dragenter events.
  {
    DOMDragEventWaiter dragleave_event_waiter("dragleave", left_frame());
    DOMDragEventWaiter dragenter_event_waiter("dragenter", right_frame());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragleave DOM event.
      std::string dragleave_event;

      // TODO(paulmeyer): https://crbug.com/669695: Need to unify coordinates
      // passed to dragend when OOPIFs are present or not.
      state->expected_dom_event_data.set_expected_client_position(
          "<no expectation>");
      state->expected_dom_event_data.set_expected_page_position(
          "<no expectation>");

      EXPECT_TRUE(
          dragleave_event_waiter.WaitForNextMatchingEvent(&dragleave_event));
      EXPECT_THAT(dragleave_event, state->expected_dom_event_data.Matches());
    }

    {  // Verify dragenter DOM event.
      std::string dragenter_event;

      // Update expected event coordinates after SimulateMouseMoveToRightFrame
      // (these coordinates are relative to the right frame).
      state->expected_dom_event_data.set_expected_client_position("(155, 150)");
      state->expected_dom_event_data.set_expected_page_position("(155, 150)");

      EXPECT_TRUE(
          dragenter_event_waiter.WaitForNextMatchingEvent(&dragenter_event));
      EXPECT_THAT(dragenter_event, state->expected_dom_event_data.Matches());
    }

    // Note that ash (unlike aura/x11) will not fire dragover event in response
    // to the same mouse event that trigerred a dragenter.  Because of that, we
    // postpone dragover testing until the next test step below.  See
    // implementation of ash::DragDropController::DragUpdate for details.
  }

  // Move the mouse twice in the right frame.  The 1st move will ensure that
  // allowed operations communicated by the renderer will be stored in
  // WebContentsViewAura::current_drag_op_.  The 2nd move will ensure that this
  // gets be copied into DesktopDragDropClientAuraX11::negotiated_operation_.
  for (int i = 0; i < 2; i++) {
    DOMDragEventWaiter dragover_event_waiter("dragover", right_frame());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragover DOM event.
      std::string dragover_event;
      EXPECT_TRUE(
          dragover_event_waiter.WaitForNextMatchingEvent(&dragover_event));
      EXPECT_THAT(dragover_event, state->expected_dom_event_data.Matches());
    }
  }

  // Release the mouse button to end the drag.
  state->drop_event_waiter.reset(new DOMDragEventWaiter("drop", right_frame()));
  state->dragend_event_waiter.reset(
      new DOMDragEventWaiter("dragend", left_frame()));
  SimulateMouseUp();
  // The test will continue in DragImageBetweenFrames_Step3.
}

void DragAndDropBrowserTest::DragImageBetweenFrames_Step3(
    DragAndDropBrowserTest::DragImageBetweenFrames_TestState* state) {
  // Verify drop DOM event.
  {
    std::string drop_event;
    EXPECT_TRUE(
        state->drop_event_waiter->WaitForNextMatchingEvent(&drop_event));
    state->drop_event_waiter.reset();
    EXPECT_THAT(drop_event, state->expected_dom_event_data.Matches());
  }

  // Verify dragend DOM event.
  {
    // TODO(lukasza): Figure out why the drop event sees different values of
    // DataTransfer.dropEffect and DataTransfer.types properties.
    state->expected_dom_event_data.set_expected_drop_effect("copy");
    state->expected_dom_event_data.set_expected_mime_types("");

    // TODO(paulmeyer): https://crbug.com/669695: Need to unify coordinates
    // passed to dragend when OOPIFs are present or not.
    state->expected_dom_event_data.set_expected_client_position(
        "<no expectation>");
    state->expected_dom_event_data.set_expected_page_position(
        "<no expectation>");

    std::string dragend_event;
    EXPECT_TRUE(
        state->dragend_event_waiter->WaitForNextMatchingEvent(&dragend_event));
    state->dragend_event_waiter.reset();
    EXPECT_THAT(dragend_event, state->expected_dom_event_data.Matches());
  }
}

INSTANTIATE_TEST_CASE_P(
    SameSiteSubframe, DragAndDropBrowserTest, ::testing::Values(false));

INSTANTIATE_TEST_CASE_P(
    CrossSiteSubframe, DragAndDropBrowserTest, ::testing::Values(true));

}  // namespace chrome
