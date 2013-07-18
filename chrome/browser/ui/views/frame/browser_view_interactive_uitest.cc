// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#endif

using views::FocusManager;

namespace {

// Returns the bounds of |view| in widget coordinates.
gfx::Rect GetRectInWidget(views::View* view) {
  return view->ConvertRectToWidget(view->GetLocalBounds());
}

}

typedef InProcessBrowserTest BrowserViewTest;

// Active window and focus testing is not reliable on Windows crbug.com/79493
// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))
#define MAYBE_FullscreenClearsFocus DISABLED_FullscreenClearsFocus
#else
#define MAYBE_FullscreenClearsFocus FullscreenClearsFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_FullscreenClearsFocus) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  FocusManager* focus_manager = browser_view->GetFocusManager();

  // Focus starts in the location bar or one of its children.
  EXPECT_TRUE(location_bar_view->Contains(focus_manager->GetFocusedView()));

  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Focus is released from the location bar.
  EXPECT_FALSE(location_bar_view->Contains(focus_manager->GetFocusedView()));
}
