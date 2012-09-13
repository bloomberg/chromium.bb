// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_view_controller.h"

#include "base/command_line.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

// Currently, the features under test are only enabled under Aura.
#if defined(USE_AURA)
#define MAYBE(x) x
#else
#define MAYBE(x) DISABLED_##x
#endif

// Avoid tests on branded Chrome where channel is set to CHANNEL_STABLE.
#define AVOID_TEST_ON_BRANDED_CHROME() {                                   \
  if (!IsInstantExtendedAPIEnabled(browser()->profile()))                  \
    return;                                                                \
}

namespace chrome {
namespace search {

class SearchViewControllerTest : public InProcessBrowserTest,
                                 public content::NotificationObserver,
                                 public SearchModelObserver {
 public:
  SearchViewControllerTest() {}
  virtual ~SearchViewControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableInstantExtendedAPI);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
#if defined(USE_AURA)
    const ViewID kOmniboxFocusViewID = VIEW_ID_OMNIBOX;
#else
    const ViewID kOmniboxFocusViewID = VIEW_ID_LOCATION_BAR;
#endif

    ASSERT_NO_FATAL_FAILURE(SetUpOmniboxData());
    chrome::FocusLocationBar(browser());
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), kOmniboxFocusViewID));

    // Record search model changes.
    browser()->search_model()->AddObserver(this);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    browser()->search_model()->RemoveObserver(this);
  }

  SearchViewController* controller() const {
#if defined(USE_AURA)
    const BrowserView* browser_view =
        static_cast<BrowserView*>(browser()->window());
    return browser_view->search_view_controller();
#else
    return NULL;
#endif  // USE_AURA
  }

  bool controller_state_is_ntp() const {
#if defined(USE_AURA)
    return SearchViewController::is_ntp_state(controller()->state());
#else
    return false;
#endif  // USE_AURA
  }

  bool controller_state_is_suggestions() const {
    return controller()->state() == SearchViewController::STATE_SUGGESTIONS;
  }

  bool controller_ntp_container_is_visible() const {
    return controller()->ntp_container_->visible();
  }

  bool controller_contents_has_overlay() const {
    return controller()->contents_container_->overlay();
  }

  bool controller_contents_overlay_visible() const {
    return controller()->contents_container_->overlay()->visible();
  }

  bool controller_contents_has_preview() const {
    return controller()->contents_container_->preview();
  }

  // Returns last mode change observed.
  Mode last_mode() const { return last_mode_; }

 private:
  // Populates bookmark model with simple data set so that omnibox suggestions
  // are non-empty.
  void SetUpOmniboxData() {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForProfile(browser()->profile());
    ASSERT_NE(static_cast<BookmarkModel*>(NULL), bookmark_model);
    if (!bookmark_model->IsLoaded()) {
      content::NotificationRegistrar registrar;
      registrar.Add(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                    content::Source<Profile>(browser()->profile()));
      content::RunMessageLoop();
    }
    bookmark_utils::AddIfNotBookmarked(bookmark_model,
                                       GURL("http://www.farmall.com"),
                                       ASCIIToUTF16("dirt"));
    bookmark_utils::AddIfNotBookmarked(bookmark_model,
                                       GURL("http://www.johndeere.com"),
                                       ASCIIToUTF16("dirty"));
    bookmark_utils::AddIfNotBookmarked(bookmark_model,
                                       GURL("http://www.kubota.com"),
                                       ASCIIToUTF16("dirtier"));
  }

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED:
        break;
      default:
        FAIL() << "Unexpected notification type.";
    }
    MessageLoop::current()->Quit();
  }

  // SearchModelObserver overrides:
  virtual void ModeChanged(const Mode& old_mode,
                           const Mode& new_mode) OVERRIDE {
    last_mode_ = new_mode;
  }

  // Recorded when |ModeChanged| is observed.
  Mode last_mode_;

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

IN_PROC_BROWSER_TEST_F(SearchViewControllerTest,
                       MAYBE(NTPToNativeSuggestions)) {
  AVOID_TEST_ON_BRANDED_CHROME();

  // Initial state.
  EXPECT_TRUE(browser()->search_model()->mode().is_default());
  ASSERT_NE(static_cast<SearchViewController*>(NULL), controller());

  // Get to NTP.
  AddTabAtIndex(0, GURL("chrome://newtab"), content::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(browser()->search_model()->mode().is_ntp());
  EXPECT_TRUE(controller_state_is_ntp());

  // Type into the omnibox.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_D,
      false, false, false, false,
      chrome::NOTIFICATION_SEARCH_VIEW_CONTROLLER_ANIMATION_FINISHED,
      content::Source<SearchViewController>(controller())));

  // Expect it to change mode to show suggestions.
  EXPECT_EQ(Mode(Mode::MODE_SEARCH_SUGGESTIONS, true), last_mode());

  // Expect that the controller state ends at |STATE_SUGGESTIONS|.
  EXPECT_TRUE(controller_state_is_suggestions());

  // Expect that the |ntp_container_| layer is hidden.
  EXPECT_FALSE(controller_ntp_container_is_visible());

  // Expect that the |contents_container_| has the overlay set, but no preview.
  ASSERT_TRUE(controller_contents_has_overlay());
  EXPECT_TRUE(controller_contents_overlay_visible());
  EXPECT_FALSE(controller_contents_has_preview());
}

IN_PROC_BROWSER_TEST_F(SearchViewControllerTest,
                       MAYBE(NTPToInstantSuggestions)) {
  AVOID_TEST_ON_BRANDED_CHROME();

  PrefService* service = browser()->profile()->GetPrefs();
  service->SetBoolean(prefs::kInstantEnabled, true);
  EXPECT_TRUE(InstantController::IsInstantEnabled(browser()->profile()));

  // Initial state.
  EXPECT_TRUE(browser()->search_model()->mode().is_default());
  ASSERT_NE(static_cast<SearchViewController*>(NULL), controller());

  // Get to NTP.
  AddTabAtIndex(0, GURL("chrome://newtab"), content::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(browser()->search_model()->mode().is_ntp());
  EXPECT_TRUE(controller_state_is_ntp());

  // Type into the omnibox.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_D,
      false, false, false, false,
      chrome::NOTIFICATION_SEARCH_VIEW_CONTROLLER_ANIMATION_FINISHED,
      content::Source<SearchViewController>(controller())));

  // Expect it to change mode to show suggestions.
  EXPECT_EQ(Mode(Mode::MODE_SEARCH_SUGGESTIONS, true), last_mode());

  // Expect that the controller state ends at |STATE_SUGGESTIONS|.
  EXPECT_TRUE(controller_state_is_suggestions());

  // Expect that the |ntp_container_| layer is hidden.
  EXPECT_FALSE(controller_ntp_container_is_visible());

  // Expect that the |contents_container_| overlay exists and but is invisible.
  ASSERT_TRUE(controller_contents_has_overlay());
  EXPECT_FALSE(controller_contents_overlay_visible());
}

}  // namespace search
}  // namespace chrome
