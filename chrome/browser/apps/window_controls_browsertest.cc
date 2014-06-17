// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager.h"

class WindowControlsTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAppWindowControls);
  }
  content::WebContents* GetWebContentsForExtensionWindow(
      const extensions::Extension* extension);
};

content::WebContents* WindowControlsTest::GetWebContentsForExtensionWindow(
    const extensions::Extension* extension) {
  extensions::ProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile())->process_manager();

  // Lookup render view host for background page.
  const extensions::ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension->id());
  content::RenderViewHost* background_view_host =
      extension_host->render_view_host();

  // Go through all active views, looking for the first window of the extension
  const extensions::ProcessManager::ViewSet all_views =
      process_manager->GetAllViews();
  extensions::ProcessManager::ViewSet::const_iterator it = all_views.begin();
  for (; it != all_views.end(); ++it) {
    content::RenderViewHost* host = *it;

    // Filter out views not part of this extension
    if (process_manager->GetExtensionForRenderViewHost(host) == extension) {
      // Filter out the background page view
      if (host != background_view_host) {
        content::WebContents* web_contents =
            content::WebContents::FromRenderViewHost(host);
        return web_contents;
      }
    }
  }
  return NULL;
}

IN_PROC_BROWSER_TEST_F(WindowControlsTest, CloseControlWorks) {
  // Launch app and wait for window to show up
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("window_controls/buttons", "window-opened");

  // Find WebContents of window
  content::WebContents* web_contents =
      GetWebContentsForExtensionWindow(extension);
  ASSERT_TRUE(web_contents != NULL);

  // Send a left click on the "Close" button and wait for the close action
  // to happen.
  ExtensionTestMessageListener window_closed("window-closed", false);

  // Send mouse click somewhere inside the [x] button
  const int controlOffset = 25;
  int x = web_contents->GetContainerBounds().size().width() - controlOffset;
  int y = controlOffset;
  content::SimulateMouseClickAt(web_contents,
                                0,
                                blink::WebMouseEvent::ButtonLeft,
                                gfx::Point(x, y));

  ASSERT_TRUE(window_closed.WaitUntilSatisfied());
}
