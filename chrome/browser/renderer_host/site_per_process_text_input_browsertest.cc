// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/text_input_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "url/gurl.h"

// TODO(ekaramad): The following tests are only active on aura platforms. After
// fixing crbug.com/578168 for all platforms, the following tests should be
// activated for other platforms, e.g., Mac and Android (crbug.com/602723).

///////////////////////////////////////////////////////////////////////////////
// TextInputManager and IME Tests
//
// The following tests verify the correctness of TextInputState tracking on the
// browser side. They also make sure the IME logic works correctly. The baseline
// for comparison is the default functionality in the non-OOPIF case (i.e., the
// legacy implementation in RWHV's other than RWHVCF).
// These tests live outside content/ because they rely on being part of the
// interactive UI test framework (to avoid flakiness).

namespace {
using IndexVector = std::vector<size_t>;

// TextInputManager Observers

// A base class for observing the TextInputManager owned by the given
// WebContents. Subclasses could observe the TextInputManager for different
// changes. The class wraps a public tester which accepts callbacks that
// are run after specific changes in TextInputManager. Different observers can
// be subclassed from this by providing their specific callback methods.
class TextInputManagerObserverBase {
 public:
  explicit TextInputManagerObserverBase(content::WebContents* web_contents)
      : tester_(new content::TextInputManagerTester(web_contents)),
        success_(false) {}

  virtual ~TextInputManagerObserverBase() {}

  // Wait for derived class's definition of success.
  void Wait() {
    if (success_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner();
    message_loop_runner_->Run();
  }

  bool success() const { return success_; }

 protected:
  content::TextInputManagerTester* tester() { return tester_.get(); }

  void OnSuccess() {
    success_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();

    // By deleting |tester_| we make sure that the internal observer used in
    // content/ is removed from the observer list of TextInputManager.
    tester_.reset(nullptr);
  }

 private:
  std::unique_ptr<content::TextInputManagerTester> tester_;
  bool success_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerObserverBase);
};

// This class observes TextInputManager for changes in |TextInputState.value|.
class TextInputManagerValueObserver : public TextInputManagerObserverBase {
 public:
  TextInputManagerValueObserver(content::WebContents* web_contents,
                                const std::string& expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(base::Bind(
        &TextInputManagerValueObserver::VerifyValue, base::Unretained(this)));
  }

 private:
  void VerifyValue(content::TextInputManagerTester* text_input_manager_tester) {
    ASSERT_EQ(tester(), text_input_manager_tester);
    std::string value;
    if (tester()->GetTextInputValue(&value) && expected_value_ == value)
      OnSuccess();
  }

  const std::string expected_value_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerValueObserver);
};

// This class observes TextInputManager for changes in |TextInputState.type|.
class TextInputManagerTypeObserver : public TextInputManagerObserverBase {
 public:
  TextInputManagerTypeObserver(content::WebContents* web_contents,
                               ui::TextInputType expected_type)
      : TextInputManagerObserverBase(web_contents),
        expected_type_(expected_type) {
    tester()->SetUpdateTextInputStateCalledCallback(base::Bind(
        &TextInputManagerTypeObserver::VerifyType, base::Unretained(this)));
  }

 private:
  void VerifyType(content::TextInputManagerTester* text_input_manager_tester) {
    ASSERT_EQ(tester(), text_input_manager_tester);
    ui::TextInputType type =
        tester()->GetTextInputType(&type) ? type : ui::TEXT_INPUT_TYPE_NONE;
    if (expected_type_ == type)
      OnSuccess();
  }

  const ui::TextInputType expected_type_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerTypeObserver);
};

// This class observes TextInputManager for the first change in TextInputState.
class TextInputManagerChangeObserver : public TextInputManagerObserverBase {
 public:
  explicit TextInputManagerChangeObserver(content::WebContents* web_contents)
      : TextInputManagerObserverBase(web_contents) {
    tester()->SetUpdateTextInputStateCalledCallback(base::Bind(
        &TextInputManagerChangeObserver::VerifyChange, base::Unretained(this)));
  }

 private:
  void VerifyChange(
      content::TextInputManagerTester* text_input_manager_tester) {
    ASSERT_EQ(tester(), text_input_manager_tester);
    if (tester()->IsTextInputStateChanged())
      OnSuccess();
  }

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerChangeObserver);
};

// This class observes |TextInputState.type| for a specific RWHV.
class ViewTextInputTypeObserver : public TextInputManagerObserverBase {
 public:
  explicit ViewTextInputTypeObserver(content::WebContents* web_contents,
                                     content::RenderWidgetHostView* rwhv,
                                     ui::TextInputType expected_type)
      : TextInputManagerObserverBase(web_contents),
        web_contents_(web_contents),
        view_(rwhv),
        expected_type_(expected_type) {
    tester()->SetUpdateTextInputStateCalledCallback(base::Bind(
        &ViewTextInputTypeObserver::VerifyType, base::Unretained(this)));
  }

 private:
  void VerifyType(content::TextInputManagerTester* tester) {
    ui::TextInputType type;
    if (!content::GetTextInputTypeForView(web_contents_, view_, &type))
      return;
    if (expected_type_ == type)
      OnSuccess();
  }

  content::WebContents* web_contents_;
  content::RenderWidgetHostView* view_;
  const ui::TextInputType expected_type_;

  DISALLOW_COPY_AND_ASSIGN(ViewTextInputTypeObserver);
};

}  // namespace

// Main class for all TextInputState and IME related tests.
class SitePerProcessTextInputManagerTest : public InProcessBrowserTest {
 public:
  SitePerProcessTextInputManagerTest() {}
  ~SitePerProcessTextInputManagerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data 'cross_site_iframe_factory.html'.
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* active_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // static
  // Adds an <input> field to a given frame by executing javascript code.
  // The input can be added as the first element or the last element of
  // |document.body|.
  static void AddInputFieldToFrame(content::RenderFrameHost* rfh,
                                   const std::string& type,
                                   const std::string& value,
                                   bool append_as_first_child) {
    std::string script = base::StringPrintf(
        "var input = document.createElement('input');"
        "input.setAttribute('type', '%s');"
        "input.setAttribute('value', '%s');"
        "document.body.%s;",
        type.c_str(), value.c_str(),
        append_as_first_child ? "insertBefore(input, document.body.firstChild)"
                              : "appendChild(input)");
    EXPECT_TRUE(ExecuteScript(rfh, script));
  }

  // Uses 'cross_site_iframe_factory.html'. The main frame's domain is
  // 'a.com'.
  void CreateIframePage(const std::string& structure) {
    std::string path = base::StringPrintf("/cross_site_iframe_factory.html?%s",
                                          structure.c_str());
    GURL main_url(embedded_test_server()->GetURL("a.com", path));
    ui_test_utils::NavigateToURL(browser(), main_url);
  }

  // Iteratively uses ChildFrameAt(frame, i) to get the i-th child frame
  // inside frame. For example, for 'a(b(c, d(e)))', [0] returns b, and
  // [0, 1, 0] returns e;
  content::RenderFrameHost* GetFrame(const IndexVector& indices) {
    content::RenderFrameHost* current = active_contents()->GetMainFrame();
    for (size_t index : indices)
      current = ChildFrameAt(current, index);
    return current;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessTextInputManagerTest);
};

// The following test loads a page with multiple nested <iframe> elements which
// are in or out of process with the main frame. Then an <input> field with
// unique value is added to every single frame on the frame tree. The test then
// creates a sequence of tab presses and verifies that after each key press, the
// TextInputState.value reflects that of the focused input, i.e., the
// TextInputManager is correctly tracking TextInputState across frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       TrackStateWhenSwitchingFocusedFrames) {
  CreateIframePage("a(a,b,c(a,b,d(e, f)),g)");
  std::vector<std::string> values{
      "main",     "node_a",   "node_b",     "node_c",     "node_c_a",
      "node_c_b", "node_c_d", "node_c_d_e", "node_c_d_f", "node_g"};

  // TODO(ekaramad): The use for explicitly constructing the IndexVector from
  // initializer list should not be necessary. However, some chromeos bots throw
  //  errors if we do not do it like this.
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}),        GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}),       GetFrame(IndexVector{2}),
      GetFrame(IndexVector{2, 0}),    GetFrame(IndexVector{2, 1}),
      GetFrame(IndexVector{2, 2}),    GetFrame(IndexVector{2, 2, 0}),
      GetFrame(IndexVector{2, 2, 1}), GetFrame(IndexVector{3})};

  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", values[i], true);

  for (size_t i = 0; i < frames.size(); ++i) {
    TextInputManagerValueObserver observer(active_contents(), values[i]);
    SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    observer.Wait();
  }
}

// The following test loads a page with two OOPIFs. An <input> is added to both
// frames and tab key is pressed until the one in the second OOPIF is focused.
// Then, the renderer processes for both frames are crashed. The test verifies
// that the TextInputManager stops tracking the RWHVs as well as properly
// resets the TextInputState after the second (active) RWHV goes away.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       StopTrackingCrashedChildFrame) {
  CreateIframePage("a(b, c)");
  std::vector<std::string> values{"node_b", "node_c"};
  std::vector<content::RenderFrameHost*> frames{GetFrame(IndexVector{0}),
                                                GetFrame(IndexVector{1})};

  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", values[i], true);

  // Tab into both inputs and make sure we correctly receive their
  // TextInputState. For the second tab two IPCs arrive: one from the first
  // frame to set the state to none, and another one from the second frame to
  // set it to TEXT. To avoid the race between them, we shall also observe the
  // first frame setting its state to NONE after the second tab.
  ViewTextInputTypeObserver view_type_observer(
      active_contents(), frames[0]->GetView(), ui::TEXT_INPUT_TYPE_NONE);

  for (size_t i = 0; i < frames.size(); ++i) {
    TextInputManagerValueObserver observer(active_contents(), values[i]);
    SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    observer.Wait();
  }

  // Make sure that the first view has set its TextInputState.type to NONE.
  view_type_observer.Wait();

  // Verify that we are tracking the TextInputState from the first frame.
  content::RenderWidgetHostView* first_view = frames[0]->GetView();
  ui::TextInputType first_view_type;
  EXPECT_TRUE(content::GetTextInputTypeForView(active_contents(), first_view,
                                               &first_view_type));
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, first_view_type);

  size_t registered_views_count =
      content::GetRegisteredViewsCountFromTextInputManager(active_contents());

  // We expect at least two views for the two child frames.
  EXPECT_GT(registered_views_count, 2U);

  // Now that the second frame's <input> is focused, we crash the first frame
  // and observe that text input state is updated for the view.
  std::unique_ptr<content::TestRenderWidgetHostViewDestructionObserver>
      destruction_observer(
          new content::TestRenderWidgetHostViewDestructionObserver(first_view));
  frames[0]->GetProcess()->Shutdown(0, false);
  destruction_observer->Wait();

  // Verify that the TextInputManager is no longer tracking TextInputState for
  // |first_view|.
  EXPECT_EQ(
      registered_views_count - 1U,
      content::GetRegisteredViewsCountFromTextInputManager(active_contents()));

  // Now crash the second <iframe> which has an active view.
  destruction_observer.reset(
      new content::TestRenderWidgetHostViewDestructionObserver(
          frames[1]->GetView()));
  frames[1]->GetProcess()->Shutdown(0, false);
  destruction_observer->Wait();

  EXPECT_EQ(
      registered_views_count - 2U,
      content::GetRegisteredViewsCountFromTextInputManager(active_contents()));
}

// The following test loads a page with two child frames: one in process and one
// out of process with main frame. The test inserts an <input> inside each frame
// and focuses the first frame and observes the TextInputManager setting the
// state to ui::TEXT_INPUT_TYPE_TEXT. Then, the frame is detached and the test
// observes that the state type is reset to ui::TEXT_INPUT_TYPE_NONE. The same
// sequence of actions is then performed on the out of process frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetStateAfterFrameDetached) {
  CreateIframePage("a(a, b)");
  std::vector<content::RenderFrameHost*> frames{GetFrame(IndexVector{0}),
                                                GetFrame(IndexVector{1})};

  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "", true);

  // Press tab key to focus the <input> in the first frame.
  TextInputManagerTypeObserver type_observer_text_a(active_contents(),
                                                    ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  type_observer_text_a.Wait();

  std::string remove_first_iframe_script =
      "var frame = document.querySelector('iframe');"
      "frame.parentNode.removeChild(frame);";
  // Detach first frame and observe |TextInputState.type| resetting to
  // ui::TEXT_INPUT_TYPE_NONE.
  TextInputManagerTypeObserver type_observer_none_a(active_contents(),
                                                    ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_TRUE(ExecuteScript(active_contents(), remove_first_iframe_script));
  type_observer_none_a.Wait();

  // Press tab to focus the <input> in the second frame.
  TextInputManagerTypeObserver type_observer_text_b(active_contents(),
                                                    ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  type_observer_text_b.Wait();

  // Detach first frame and observe |TextInputState.type| resetting to
  // ui::TEXT_INPUT_TYPE_NONE.
  TextInputManagerTypeObserver type_observer_none_b(active_contents(),
                                                    ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_TRUE(ExecuteScript(active_contents(), remove_first_iframe_script));
  type_observer_none_b.Wait();
}

// This test creates a page with one OOPIF and adds an <input> to it. Then, the
// <input> is focused and the test verfies that the |TextInputState.type| is set
// to ui::TEXT_INPUT_TYPE_TEXT. Next, the child frame is navigated away and the
// test verifies that |TextInputState.type| resets to ui::TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetStateAfterChildNavigation) {
  CreateIframePage("a(b)");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  content::RenderFrameHost* child_frame = GetFrame(IndexVector{0});

  AddInputFieldToFrame(child_frame, "text", "child", false);

  // Focus <input> in child frame and verify the |TextInputState.value|.
  TextInputManagerValueObserver child_set_state_observer(active_contents(),
                                                         "child");
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  child_set_state_observer.Wait();

  // Navigate the child frame to about:blank and verify that TextInputManager
  // correctly sets its |TextInputState.type| to ui::TEXT_INPUT_TYPE_NONE.
  TextInputManagerTypeObserver child_reset_state_observer(
      active_contents(), ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_TRUE(ExecuteScript(
      main_frame, "document.querySelector('iframe').src = 'about:blank'"));
  child_reset_state_observer.Wait();
}

// This test creates a blank page and adds an <input> to it. Then, the <input>
// is focused and the test verfies that the |TextInputState.type| is set to
// ui::TEXT_INPUT_TYPE_TEXT. Next, the browser is navigated away and the test
// verifies that |TextInputState.type| resets to ui::TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetStateAfterBrowserNavigation) {
  CreateIframePage("a()");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  AddInputFieldToFrame(main_frame, "text", "", false);

  TextInputManagerTypeObserver set_state_observer(active_contents(),
                                                  ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  set_state_observer.Wait();

  TextInputManagerTypeObserver reset_state_observer(active_contents(),
                                                    ui::TEXT_INPUT_TYPE_NONE);
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  reset_state_observer.Wait();
}

// TODO(ekaramad): The following tests are specifically written for Aura and are
// based on InputMethodObserver. Write similar tests for Mac/Android/Mus
// (crbug.com/602723).

// Observes current input method for state changes.
class InputMethodObserverBase {
 public:
  explicit InputMethodObserverBase(content::WebContents* web_contents)
      : success_(false),
        test_observer_(content::TestInputMethodObserver::Create(web_contents)) {
  }

  void Wait() {
    if (success_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner();
    message_loop_runner_->Run();
  }

  bool success() const { return success_; }

 protected:
  content::TestInputMethodObserver* test_observer() {
    return test_observer_.get();
  }

  const base::Closure success_closure() {
    return base::Bind(&InputMethodObserverBase::OnSuccess,
                      base::Unretained(this));
  }

 private:
  void OnSuccess() {
    success_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  bool success_;
  std::unique_ptr<content::TestInputMethodObserver> test_observer_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodObserverBase);
};

class InputMethodObserverForShowIme : public InputMethodObserverBase {
 public:
  explicit InputMethodObserverForShowIme(content::WebContents* web_contents)
      : InputMethodObserverBase(web_contents) {
    test_observer()->SetOnShowImeIfNeededCallback(success_closure());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodObserverForShowIme);
};

// This test verifies that the IME for Aura is shown if and only if the current
// client's |TextInputState.type| is not ui::TEXT_INPUT_TYPE_NONE and the flag
// |TextInputState.show_ime_if_needed| is true. This should happen even when
// the TextInputState has not changed (according to the platform), e.g., in
// aura when receiving two consecutive updates with same |TextInputState.type|.
// TODO(ekaramad): This test is actually a unit test not necessarily an OOPIF
// test. We should move it to somewhere more relevant.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       CorrectlyShowImeIfNeeded) {
  // We only need the <iframe> page to create RWHV.
  CreateIframePage("a()");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  content::RenderWidgetHostView* view = main_frame->GetView();
  content::WebContents* web_contents = active_contents();

  content::TextInputStateSender sender(view);

  auto send_and_check_show_ime = [&sender, &web_contents]() {
    InputMethodObserverForShowIme observer(web_contents);
    sender.Send();
    return observer.success();
  };

  // Sending an empty state should not trigger ime.
  EXPECT_FALSE(send_and_check_show_ime());

  // Set |TextInputState.type| to text. Expect no IME.
  sender.SetType(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_FALSE(send_and_check_show_ime());

  // Set |TextInputState.show_ime_if_needed| to true. Expect IME.
  sender.SetShowImeIfNeeded(true);
  EXPECT_TRUE(send_and_check_show_ime());

  // Send the same message. Expect IME (no change).
  EXPECT_TRUE(send_and_check_show_ime());

  // Reset |TextInputState.show_ime_if_needed|. Expect no IME.
  sender.SetShowImeIfNeeded(false);
  EXPECT_FALSE(send_and_check_show_ime());

  // Setting an irrelevant field. Expect no IME.
  sender.SetMode(ui::TEXT_INPUT_MODE_LATIN);
  EXPECT_FALSE(send_and_check_show_ime());

  // Set |TextInputState.show_ime_if_needed|. Expect IME.
  sender.SetShowImeIfNeeded(true);
  EXPECT_TRUE(send_and_check_show_ime());

  // Set |TextInputState.type| to ui::TEXT_INPUT_TYPE_NONE. Expect no IME.
  sender.SetType(ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_FALSE(send_and_check_show_ime());
}
