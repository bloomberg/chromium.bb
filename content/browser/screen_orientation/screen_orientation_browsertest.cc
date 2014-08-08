// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/command_line.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/compositor/compositor_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif // OS_WIN

namespace content {

class ScreenOrientationBrowserTest : public ContentBrowserTest  {
 public:
  ScreenOrientationBrowserTest() {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  virtual void SetUp() OVERRIDE {
    // Painting has to happen otherwise the Resize messages will be added on top
    // of each other without properly ack-painting which will fail and crash.
    UseSoftwareCompositing();

    ContentBrowserTest::SetUp();
  }

 protected:
  void SendFakeScreenOrientation(unsigned angle, const std::string& strType) {
    RenderWidgetHost* rwh = shell()->web_contents()->GetRenderWidgetHostView()
        ->GetRenderWidgetHost();
    blink::WebScreenInfo screen_info;
    rwh->GetWebScreenInfo(&screen_info);
    screen_info.orientationAngle = angle;

    blink::WebScreenOrientationType type = blink::WebScreenOrientationUndefined;
    if (strType == "portrait-primary") {
      type = blink::WebScreenOrientationPortraitPrimary;
    } else if (strType == "portrait-secondary") {
      type = blink::WebScreenOrientationPortraitSecondary;
    } else if (strType == "landscape-primary") {
      type = blink::WebScreenOrientationLandscapePrimary;
    } else if (strType == "landscape-secondary") {
      type = blink::WebScreenOrientationLandscapeSecondary;
    }
    ASSERT_NE(blink::WebScreenOrientationUndefined, type);
    screen_info.orientationType = type;

    ViewMsg_Resize_Params params;
    params.screen_info = screen_info;
    params.new_size = gfx::Size(0, 0);
    params.physical_backing_size = gfx::Size(300, 300);
    params.overdraw_bottom_height = 0.f;
    params.resizer_rect = gfx::Rect();
    params.is_fullscreen = false;
    rwh->Send(new ViewMsg_Resize(rwh->GetRoutingID(), params));
  }

  int GetOrientationAngle() {
    int angle;
    ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                             "screen.orientation.angle")->GetAsInteger(&angle);
    return angle;
  }

  std::string GetOrientationType() {
    std::string type;
    ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                             "screen.orientation.type")->GetAsString(&type);
    return type;
  }

  bool ScreenOrientationSupported() {
    bool support;
    ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                             "'orientation' in screen")->GetAsBoolean(&support);
    return support;
  }

  bool WindowOrientationSupported() {
    bool support;
    ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                             "'orientation' in window")->GetAsBoolean(&support);
    return support;
  }

  int GetWindowOrientationAngle() {
    int angle;
    ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                             "window.orientation")->GetAsInteger(&angle);
    return angle;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationBrowserTest);
};

// This test doesn't work on MacOS X but the reason is mostly because it is not
// used Aura. It could be set as !defined(OS_MACOSX) but the rule below will
// actually support MacOS X if and when it switches to Aura.
#if defined(USE_AURA) || defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest, ScreenOrientationChange) {
  std::string types[] = { "portrait-primary",
                          "portrait-secondary",
                          "landscape-primary",
                          "landscape-secondary" };
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_screenorientationchange.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();
#if USE_AURA
  WaitForResizeComplete(shell()->web_contents());
#endif // USE_AURA

#if defined(OS_WIN)
  // Screen Orientation is currently disabled on Windows 8.
  // This test will break, requiring an update when the API will be enabled.
  if (base::win::OSInfo::GetInstance()->version() >= base::win::VERSION_WIN8) {
    EXPECT_EQ(false, ScreenOrientationSupported());
    return;
  }
#endif // defined(OS_WIN)

  int angle = GetOrientationAngle();

  for (int i = 0; i < 4; ++i) {
    angle = (angle + 90) % 360;
    SendFakeScreenOrientation(angle, types[i]);

    TestNavigationObserver navigation_observer(shell()->web_contents());
    navigation_observer.Wait();
    EXPECT_EQ(angle, GetOrientationAngle());
    EXPECT_EQ(types[i], GetOrientationType());
  }
}
#endif // defined(USE_AURA) || defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest, WindowOrientationChange) {
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_windoworientationchange.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();
#if USE_AURA
  WaitForResizeComplete(shell()->web_contents());
#endif // USE_AURA

  if (!WindowOrientationSupported())
    return;

  int angle = GetWindowOrientationAngle();

  for (int i = 0; i < 4; ++i) {
    angle = (angle + 90) % 360;
    SendFakeScreenOrientation(angle, "portrait-primary");

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    navigation_observer.Wait();
    EXPECT_EQ(angle == 270 ? -90 : angle, GetWindowOrientationAngle());
  }
}

// Chromium Android does not support fullscreen
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest, LockSmoke) {
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_lock_smoke.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(test_url);

#if defined(OS_WIN)
  // Screen Orientation is currently disabled on Windows 8.
  // This test will break, requiring an update when the API will be enabled.
  if (base::win::OSInfo::GetInstance()->version() >= base::win::VERSION_WIN8) {
    EXPECT_EQ(false, ScreenOrientationSupported());
    return;
  }
#endif // defined(OS_WIN)

  navigation_observer.Wait();
#if USE_AURA
  WaitForResizeComplete(shell()->web_contents());
#endif // USE_AURA

  std::string expected =
#if defined(OS_ANDROID)
      "SecurityError"; // WebContents need to be fullscreen.
#else
      "NotSupportedError"; // Locking isn't supported.
#endif

  EXPECT_EQ(expected, shell()->web_contents()->GetLastCommittedURL().ref());
}

} // namespace content
