// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/memory/tab_manager_observer.h"
#include "chrome/browser/memory/tab_manager_web_contents_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using content::OpenURLParams;
using content::WebContents;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

namespace memory {

class TabManagerObserverTest : public InProcessBrowserTest {
 public:
  TabManagerObserverTest() {}

  // Helper functions.
  void set_tab_strip_model(TabStripModel* tsm) { tab_strip_model_ = tsm; }

  int GetIndex(WebContents* contents) {
    return tab_strip_model_->GetIndexOfWebContents(contents);
  }

  WebContents* GetContents(int index) {
    return tab_strip_model_->GetWebContentsAt(index);
  }

  int64_t ContentsId(WebContents* contents) {
    return TabManager::IdFromWebContents(contents);
  }

 private:
  TabStripModel* tab_strip_model_;
};

class MockTabManagerObserver : public TabManagerObserver {
 public:
  MockTabManagerObserver()
      : nb_events_(0),
        contents_(nullptr),
        is_discarded_(false),
        is_auto_discardable_(true) {}

  // TabManagerObserver implementation:
  void OnDiscardedStateChange(content::WebContents* contents,
                              bool is_discarded) override {
    nb_events_++;
    contents_ = contents;
    is_discarded_ = is_discarded;
  }

  void OnAutoDiscardableStateChange(content::WebContents* contents,
                                    bool is_auto_discardable) override {
    nb_events_++;
    contents_ = contents;
    is_auto_discardable_ = is_auto_discardable;
  }

  int nb_events() const { return nb_events_; }
  WebContents* content() const { return contents_; }
  bool is_discarded() const { return is_discarded_; }
  bool is_auto_discardable() const { return is_auto_discardable_; }

 private:
  int nb_events_;
  WebContents* contents_;
  bool is_discarded_;
  bool is_auto_discardable_;

  DISALLOW_COPY_AND_ASSIGN(MockTabManagerObserver);
};

IN_PROC_BROWSER_TEST_F(TabManagerObserverTest, OnDiscardStateChange) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  auto* tsm = browser()->tab_strip_model();
  set_tab_strip_model(tsm);

  // Open two tabs.
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  int index_1 = GetIndex(browser()->OpenURL(open1));

  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  int index_2 = GetIndex(browser()->OpenURL(open2));

  // Subscribe observer to TabManager's observer list.
  MockTabManagerObserver tabmanager_observer;
  tab_manager->AddObserver(&tabmanager_observer);

  // Discards both tabs and make sure the events were observed properly.
  EXPECT_TRUE(tab_manager->DiscardTabById(ContentsId(GetContents(index_1))));
  EXPECT_EQ(1, tabmanager_observer.nb_events());
  EXPECT_EQ(ContentsId(GetContents(index_1)),
            ContentsId(tabmanager_observer.content()));
  EXPECT_TRUE(tabmanager_observer.is_discarded());

  EXPECT_TRUE(tab_manager->DiscardTabById(ContentsId(GetContents(index_2))));
  EXPECT_EQ(2, tabmanager_observer.nb_events());
  EXPECT_EQ(ContentsId(GetContents(index_2)),
            ContentsId(tabmanager_observer.content()));
  EXPECT_TRUE(tabmanager_observer.is_discarded());

  // Discarding an already discarded tab shouldn't fire the observers.
  EXPECT_FALSE(tab_manager->DiscardTabById(ContentsId(GetContents(index_1))));
  EXPECT_EQ(2, tabmanager_observer.nb_events());

  // Reload tab 1.
  tsm->ActivateTabAt(index_1, false);
  EXPECT_EQ(index_1, tsm->active_index());
  EXPECT_EQ(3, tabmanager_observer.nb_events());
  EXPECT_EQ(ContentsId(GetContents(index_1)),
            ContentsId(tabmanager_observer.content()));
  EXPECT_FALSE(tabmanager_observer.is_discarded());

  // Reloading a tab that's not discarded shouldn't fire the observers.
  tsm->ActivateTabAt(index_1, false);
  EXPECT_EQ(3, tabmanager_observer.nb_events());

  // Reload tab 2.
  tsm->ActivateTabAt(index_2, false);
  EXPECT_EQ(index_2, tsm->active_index());
  EXPECT_EQ(4, tabmanager_observer.nb_events());
  EXPECT_EQ(ContentsId(GetContents(index_2)),
            ContentsId(tabmanager_observer.content()));
  EXPECT_FALSE(tabmanager_observer.is_discarded());

  // After removing the observer from the TabManager's list, it shouldn't
  // receive events anymore.
  tab_manager->RemoveObserver(&tabmanager_observer);
  EXPECT_TRUE(tab_manager->DiscardTabById(ContentsId(GetContents(index_1))));
  EXPECT_EQ(4, tabmanager_observer.nb_events());
}

IN_PROC_BROWSER_TEST_F(TabManagerObserverTest, OnAutoDiscardableStateChange) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  auto* tsm = browser()->tab_strip_model();
  set_tab_strip_model(tsm);

  // Open two tabs.
  OpenURLParams open(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                     WindowOpenDisposition::NEW_BACKGROUND_TAB,
                     ui::PAGE_TRANSITION_TYPED, false);
  WebContents* contents = browser()->OpenURL(open);

  // Subscribe observer to TabManager's observer list.
  MockTabManagerObserver observer;
  tab_manager->AddObserver(&observer);

  // No events initially.
  EXPECT_EQ(0, observer.nb_events());

  // Should maintain at zero since the default value of the state is true.
  tab_manager->SetTabAutoDiscardableState(contents, true);
  EXPECT_EQ(0, observer.nb_events());

  // Now it has to change.
  tab_manager->SetTabAutoDiscardableState(contents, false);
  EXPECT_EQ(1, observer.nb_events());
  EXPECT_FALSE(observer.is_auto_discardable());
  EXPECT_EQ(ContentsId(contents), ContentsId(observer.content()));

  // No changes since it's not a new state.
  tab_manager->SetTabAutoDiscardableState(contents, false);
  EXPECT_EQ(1, observer.nb_events());

  // Change it back and we should have another event.
  tab_manager->SetTabAutoDiscardableState(contents, true);
  EXPECT_EQ(2, observer.nb_events());
  EXPECT_TRUE(observer.is_auto_discardable());
  EXPECT_EQ(ContentsId(contents), ContentsId(observer.content()));
}

}  // namespace memory

#endif  // OS_WIN || OS_MAXOSX || OS_LINUX
