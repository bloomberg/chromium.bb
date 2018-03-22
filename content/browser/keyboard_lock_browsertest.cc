// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/native_widget_types.h"

#ifdef USE_AURA
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#endif  // USE_AURA

namespace content {

namespace {

// TODO(joedow): Enable tests on additional platforms as they are implemented.
#if defined(OS_WIN)
#define MAYBE_RUN(test_name) test_name
#else
#define MAYBE_RUN(test_name) DISABLED_##test_name
#endif

constexpr char kFullscreenFrameName[] = "/fullscreen_frame.html";

constexpr char kKeyboardLockMethodExistanceCheck[] =
    "window.domAutomationController.send("
    "  (navigator.keyboard != undefined) &&"
    "  (navigator.keyboard.lock != undefined));";

constexpr char kKeyboardLockMethodCallWithAllKeys[] =
    "navigator.keyboard.lock().then("
    "  () => { window.domAutomationController.send(true); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

constexpr char kKeyboardLockMethodCallWithSomeKeys[] =
    "navigator.keyboard.lock(['MetaLeft', 'Tab', 'AltLeft']).then("
    "  () => { window.domAutomationController.send(true); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

constexpr char kKeyboardLockMethodCallWithAllInvalidKeys[] =
    "navigator.keyboard.lock(['BlerghLeft', 'BlargRight']).then("
    "  () => { window.domAutomationController.send(true); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

constexpr char kKeyboardLockMethodCallWithSomeInvalidKeys[] =
    "navigator.keyboard.lock(['Tab', 'BlarghTab', 'Space', 'BlerghLeft']).then("
    "  () => { window.domAutomationController.send(true); },"
    "  () => { window.domAutomationController.send(false); },"
    ");";

constexpr char kKeyboardUnlockMethodCall[] = "navigator.keyboard.unlock()";

bool g_window_has_focus = false;

#if defined(USE_AURA)
class TestRenderWidgetHostView : public RenderWidgetHostViewAura {
 public:
  TestRenderWidgetHostView(RenderWidgetHost* host, bool is_guest_view_hack)
      : RenderWidgetHostViewAura(host,
                                 is_guest_view_hack,
                                 false /* is_mus_browser_plugin_guest */) {}
  ~TestRenderWidgetHostView() override {}

  bool HasFocus() const override { return g_window_has_focus; }

  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    // Ignore all focus events.
  }
};

void InstallCreateHooksForKeyboardLockBrowserTests() {
  WebContentsViewAura::InstallCreateHookForTests(
      [](RenderWidgetHost* host,
         bool is_guest_view_hack) -> RenderWidgetHostViewAura* {
        return new TestRenderWidgetHostView(host, is_guest_view_hack);
      });
}
#endif  // USE_AURA

class FakeKeyboardLockWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeKeyboardLockWebContentsDelegate() {}
  ~FakeKeyboardLockWebContentsDelegate() override {}

  // WebContentsDelegate overrides.
  void EnterFullscreenModeForTab(WebContents* web_contents,
                                 const GURL& origin) override;
  void ExitFullscreenModeForTab(WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const override;

 private:
  bool is_fullscreen_ = false;
};

void FakeKeyboardLockWebContentsDelegate::EnterFullscreenModeForTab(
    WebContents* web_contents,
    const GURL& origin) {
  is_fullscreen_ = true;
}

void FakeKeyboardLockWebContentsDelegate::ExitFullscreenModeForTab(
    WebContents* web_contents) {
  is_fullscreen_ = false;
}

bool FakeKeyboardLockWebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  return is_fullscreen_;
}

}  // namespace

class KeyboardLockBrowserTest : public ContentBrowserTest {
 public:
  KeyboardLockBrowserTest();
  ~KeyboardLockBrowserTest() override;

 protected:
  // ContentBrowserTest overrides.
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  virtual void SetUpFeatureList();

  // Helper methods for common tasks.
  bool KeyboardLockApiExists();
  void NavigateToTestURL();
  void RequestKeyboardLock(const base::Location& from_here,
                           bool lock_all_keys = true);
  void CancelKeyboardLock(const base::Location& from_here);
  void EnterFullscreen(const base::Location& from_here);
  void ExitFullscreen(const base::Location& from_here);
  void FocusContent(const base::Location& from_here);
  void BlurContent(const base::Location& from_here);
  void VerifyKeyboardLockState(const base::Location& from_here);

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

  GURL https_fullscreen_frame() {
    return https_test_server()->GetURL(kFullscreenFrameName);
  }

  base::test::ScopedFeatureList* feature_list() {
    return &scoped_feature_list_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_test_server_;
  FakeKeyboardLockWebContentsDelegate web_contents_delegate_;
};

KeyboardLockBrowserTest::KeyboardLockBrowserTest()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

KeyboardLockBrowserTest::~KeyboardLockBrowserTest() = default;

void KeyboardLockBrowserTest::SetUp() {
  // Assume we have focus to start with.
  g_window_has_focus = true;
#if defined(USE_AURA)
  InstallCreateHooksForKeyboardLockBrowserTests();
#endif
  SetUpFeatureList();
  ContentBrowserTest::SetUp();
}

void KeyboardLockBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);
}

void KeyboardLockBrowserTest::SetUpOnMainThread() {
  web_contents()->SetDelegate(&web_contents_delegate_);

  // KeyboardLock requires a secure context (HTTPS).
  https_test_server()->AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_test_server()->Start());
}

void KeyboardLockBrowserTest::SetUpFeatureList() {
  feature_list()->InitAndEnableFeature(features::kKeyboardLockAPI);
}

bool KeyboardLockBrowserTest::KeyboardLockApiExists() {
  bool api_exists = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), kKeyboardLockMethodExistanceCheck, &api_exists));
  return api_exists;
}

void KeyboardLockBrowserTest::NavigateToTestURL() {
  ASSERT_TRUE(NavigateToURL(shell(), https_fullscreen_frame()));

  ASSERT_TRUE(KeyboardLockApiExists());

  // Ensure the window has focus and is in windowed mode after the navigation.
  FocusContent(FROM_HERE);
  ExitFullscreen(FROM_HERE);
}

void KeyboardLockBrowserTest::RequestKeyboardLock(
    const base::Location& from_here,
    bool lock_all_keys /*=true*/) {
  bool result;
  // keyboardLock() is an async call which requires a promise handling dance.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      lock_all_keys ? kKeyboardLockMethodCallWithAllKeys
                    : kKeyboardLockMethodCallWithSomeKeys,
      &result))
      << "Location: " << from_here.ToString();

  ASSERT_TRUE(result) << "Location: " << from_here.ToString();

  ASSERT_EQ(result, web_contents()->GetKeyboardLockWidget() != nullptr)
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::CancelKeyboardLock(
    const base::Location& from_here) {
  // keyboardUnlock() is a synchronous call.
  ASSERT_TRUE(ExecuteScript(web_contents(), kKeyboardUnlockMethodCall));

  ASSERT_EQ(nullptr, web_contents()->GetKeyboardLockWidget())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::EnterFullscreen(const base::Location& from_here) {
  web_contents()->EnterFullscreenMode(https_fullscreen_frame());

  ASSERT_TRUE(web_contents()->IsFullscreenForCurrentTab())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::ExitFullscreen(const base::Location& from_here) {
  web_contents()->ExitFullscreenMode(/*should_resize=*/true);

  ASSERT_FALSE(web_contents()->IsFullscreenForCurrentTab())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::FocusContent(const base::Location& from_here) {
  g_window_has_focus = true;
  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  host->GotFocus();
  host->SetActive(true);

  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->HasFocus())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::BlurContent(const base::Location& from_here) {
  g_window_has_focus = false;
  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  host->SetActive(false);
  host->LostFocus();

  ASSERT_FALSE(web_contents()->GetRenderWidgetHostView()->HasFocus())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::VerifyKeyboardLockState(
    const base::Location& from_here) {
  bool keyboard_lock_requested = !!web_contents()->GetKeyboardLockWidget();

  bool ux_conditions_satisfied =
      web_contents()->GetRenderWidgetHostView()->HasFocus() &&
      web_contents()->IsFullscreenForCurrentTab();

  bool keyboard_lock_active =
      web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked();

  // Keyboard lock only active when requested and the UX is in the right state.
  ASSERT_EQ(keyboard_lock_active,
            ux_conditions_satisfied && keyboard_lock_requested)
      << "Location: " << from_here.ToString();
}

class KeyboardLockDisabledBrowserTest : public KeyboardLockBrowserTest {
 public:
  KeyboardLockDisabledBrowserTest() = default;
  ~KeyboardLockDisabledBrowserTest() override = default;

 protected:
  // KeyboardLockBrowserTest override.
  void SetUpFeatureList() override;
};

void KeyboardLockDisabledBrowserTest::SetUpFeatureList() {
  feature_list()->InitAndDisableFeature(features::kKeyboardLockAPI);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MAYBE_RUN(SingleLockCall)) {
  NavigateToTestURL();
  RequestKeyboardLock(FROM_HERE);
  // Don't explicitly call CancelKeyboardLock().
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(SingleLockCallForSomeKeys)) {
  NavigateToTestURL();
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  // Don't explicitly call CancelKeyboardLock().
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(SingleLockWithCancelCall)) {
  NavigateToTestURL();
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockCalledBeforeFullscreen)) {
  NavigateToTestURL();
  RequestKeyboardLock(FROM_HERE);
  EnterFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockCalledAfterFullscreen)) {
  NavigateToTestURL();
  EnterFullscreen(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockAndCancelCyclingNoActivation)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockAndCancelCyclingInFullscreen)) {
  NavigateToTestURL();

  EnterFullscreen(FROM_HERE);

  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MAYBE_RUN(CancelInFullscreen)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  ExitFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(EnterAndExitFullscreenCycling)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);

  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(GainAndLoseFocusInWindowMode)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);

  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(EnterFullscreenWithoutFocus)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);

  BlurContent(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);

  EnterFullscreen(FROM_HERE);
  FocusContent(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(GainAndLoseFocusCyclingInFullscreen)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);

  BlurContent(FROM_HERE);
  EnterFullscreen(FROM_HERE);

  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);

  ExitFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MAYBE_RUN(CancelWithoutLock)) {
  NavigateToTestURL();
  CancelKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MAYBE_RUN(MultipleLockCalls)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(MultipleCancelCalls)) {
  NavigateToTestURL();

  RequestKeyboardLock(FROM_HERE);

  CancelKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockCallWithAllInvalidKeys)) {
  NavigateToTestURL();

  bool result;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), kKeyboardLockMethodCallWithAllInvalidKeys, &result));
  ASSERT_TRUE(result);

  // If no valid Keys are passed in, then KeyboardLock will not be requested.
  ASSERT_EQ(nullptr, web_contents()->GetKeyboardLockWidget());

  EnterFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockCallWithSomeInvalidKeys)) {
  NavigateToTestURL();

  bool result;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), kKeyboardLockMethodCallWithSomeInvalidKeys, &result));
  ASSERT_TRUE(result);

  // If some valid Keys are passed in, then KeyboardLock will be requested.
  ASSERT_NE(nullptr, web_contents()->GetKeyboardLockWidget());

  EnterFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockDisabledBrowserTest,
                       MAYBE_RUN(NoKeyboardLockWhenDisabled)) {
  ASSERT_TRUE(NavigateToURL(shell(), https_fullscreen_frame()));
  ASSERT_FALSE(KeyboardLockApiExists());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(KeyboardLockNotAllowedForIFrame)) {
  // TODO(joedow): IMPLEMENT.
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(KeyboardUnlockedWhenNavigatingAway)) {
  // TODO(joedow): IMPLEMENT.
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(CrossOriginIFrameDoesNotReceiveInput)) {
  // TODO(joedow): Added per code review feedback.
  // Steps: Main frame initiates keyboard lock and goes fullscreen.  Is input
  // delivered to cross-origin iFrame?
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(CrossOriginIFrameRequestsFullscreen)) {
  // TODO(joedow): Added per code review feedback.
  // Steps: Main frame requests keyboard lock, cross-origin iFrame goes
  // fullscreen.  Should KeyboardLock be triggered in that case (presumably no).
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockRequestWhileIFrameIsFullscreen)) {
  // TODO(joedow): Added per code review feedback.
  // Steps: 1. Load a page with a cross-site iframe: call main frame "A" and the
  //           subframe "B"
  //        2. B goes fullscreen.
  //        3. A initiates keyboard lock.
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       MAYBE_RUN(LockRequestFailsFromInnerWebContents)) {
  // TODO(joedow): Added per code review feedback.
  // Steps: Try requesting KeyboardLock from with an inner WebContents context.
  // See: CreateAndAttachInnerContents() helper method in
  // https://cs.chromium.org/chromium/src/content/public/test/test_utils.h
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MAYBE_RUN(HistogramTest)) {
  // TODO(joedow): Added per code review feedback.
  // Steps: Call the API methods and verify the histogram data is accurate using
  // base::HistogramTester.  Alternatively, this could be integrated with the
  // test fixture as well.  window_open_apitest.cc (line 317()) is an example.
}

}  // namespace content
