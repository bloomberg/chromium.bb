// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_view_controller.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"

// Currently, the features under test are only enabled under Aura.
#if defined(USE_AURA)
#define MAYBE(x) x
#else
#define MAYBE(x) DISABLED_##x
#endif

// Avoid tests on branded Chrome where channel is set to CHANNEL_STABLE.
#define AVOID_TEST_ON_BRANDED_CHROME() {                                   \
  if (!chrome::search::IsInstantExtendedAPIEnabled(browser()->profile()))  \
    return;                                                                \
}

class SearchViewControllerTest : public InProcessBrowserTest {
 public:
  SearchViewControllerTest() {}
  virtual ~SearchViewControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableInstantExtendedAPI);
  }

  SearchViewController* controller() {
#if defined(USE_AURA)
    const BrowserView* browser_view =
        static_cast<BrowserView*>(browser()->window());
    return browser_view->search_view_controller();
#else
    return NULL;
#endif  // USE_AURA
  }

  bool controller_state_is_ntp() {
#if defined(USE_AURA)
    return SearchViewController::is_ntp_state(controller()->state());
#else
    return false;
#endif  // USE_AURA
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(SearchViewControllerTest, MAYBE(StartToNTP)) {
  AVOID_TEST_ON_BRANDED_CHROME();

  // Initial state.
  EXPECT_TRUE(browser()->search_model()->mode().is_default());
  ASSERT_NE(static_cast<SearchViewController*>(NULL), controller());

  AddTabAtIndex(0, GURL("chrome://newtab"), content::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(browser()->search_model()->mode().is_ntp());
  EXPECT_TRUE(controller_state_is_ntp());
}
