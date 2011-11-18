// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_strip_model.h"

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/property_bag.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/test/notification_observer_mock.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;

namespace {

// Class used to delete a TabContents when another TabContents is destroyed.
class DeleteTabContentsOnDestroyedObserver
    : public content::NotificationObserver {
 public:
  DeleteTabContentsOnDestroyedObserver(TabContentsWrapper* source,
                                       TabContentsWrapper* tab_to_delete)
      : source_(source),
        tab_to_delete_(tab_to_delete) {
    registrar_.Add(this,
                   content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                   content::Source<TabContents>(source->tab_contents()));
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    TabContentsWrapper* tab_to_delete = tab_to_delete_;
    tab_to_delete_ = NULL;
    delete tab_to_delete;
  }

 private:
  TabContentsWrapper* source_;
  TabContentsWrapper* tab_to_delete_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DeleteTabContentsOnDestroyedObserver);
};

}  // namespace

class TabStripDummyDelegate : public TabStripModelDelegate {
 public:
  explicit TabStripDummyDelegate(TabContentsWrapper* dummy)
      : dummy_contents_(dummy), can_close_(true), run_unload_(false) {}
  virtual ~TabStripDummyDelegate() {}

  void set_can_close(bool value) { can_close_ = value; }
  void set_run_unload_listener(bool value) { run_unload_ = value; }

  // Overridden from TabStripModelDelegate:
  virtual TabContentsWrapper* AddBlankTab(bool foreground) {
    return NULL;
  }
  virtual TabContentsWrapper* AddBlankTabAt(int index, bool foreground) {
    return NULL;
  }
  virtual Browser* CreateNewStripWithContents(TabContentsWrapper* contents,
                                              const gfx::Rect& window_bounds,
                                              const DockInfo& dock_info,
                                              bool maximize) {
    return NULL;
  }
  virtual void ContinueDraggingDetachedTab(TabContentsWrapper* contents,
                                           const gfx::Rect& window_bounds,
                                           const gfx::Rect& tab_bounds) {
  }
  virtual int GetDragActions() const { return 0; }
  virtual TabContentsWrapper* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      content::PageTransition transition,
      bool defer_load,
      SiteInstance* instance) const {
    if (url == GURL(chrome::kChromeUINewTabURL))
      return dummy_contents_;
    return NULL;
  }
  virtual bool CanDuplicateContentsAt(int index) { return false; }
  virtual void DuplicateContentsAt(int index) {}
  virtual void CloseFrameAfterDragSession() {}
  virtual void CreateHistoricalTab(TabContentsWrapper* contents) {}
  virtual bool RunUnloadListenerBeforeClosing(TabContentsWrapper* contents) {
    return run_unload_;
  }
  virtual bool CanRestoreTab() { return false; }
  virtual void RestoreTab() {}
  virtual bool CanCloseContents(std::vector<int>* indices) {
    if (!can_close_)
      indices->clear();
    return can_close_;
  }
  virtual bool CanBookmarkAllTabs() const { return false; }
  virtual void BookmarkAllTabs() {}
  virtual bool CanCloseTab() const { return true; }
  virtual bool LargeIconsPermitted() const { return true; }

 private:
  // A dummy TabContents we give to callers that expect us to actually build a
  // Destinations tab for them.
  TabContentsWrapper* dummy_contents_;

  // Whether tabs can be closed.
  bool can_close_;

  // Whether to report that we need to run an unload listener before closing.
  bool run_unload_;

  DISALLOW_COPY_AND_ASSIGN(TabStripDummyDelegate);
};

class TabStripModelTest : public ChromeRenderViewHostTestHarness {
 public:
  TabStripModelTest() : browser_thread_(BrowserThread::UI, &message_loop_) {
  }

  TabContentsWrapper* CreateTabContents() {
    return Browser::TabContentsFactory(profile(), NULL, 0, NULL, NULL);
  }

  TabContentsWrapper* CreateTabContentsWithSharedRPH(
      TabContents* tab_contents) {
    TabContentsWrapper* retval = Browser::TabContentsFactory(profile(),
        tab_contents->render_view_host()->site_instance(), MSG_ROUTING_NONE,
        NULL, NULL);
    EXPECT_EQ(retval->tab_contents()->GetRenderProcessHost(),
              tab_contents->GetRenderProcessHost());
    return retval;
  }

  // Forwards a URL "load" request through to our dummy TabContents
  // implementation.
  void LoadURL(TabContents* con, const std::wstring& url) {
    controller().LoadURL(GURL(WideToUTF16(url)), GURL(),
                         content::PAGE_TRANSITION_LINK, std::string());
  }

  void GoBack(TabContents* contents) {
    controller().GoBack();
  }

  void GoForward(TabContents* contents) {
    controller().GoForward();
  }

  void SwitchTabTo(TabContents* contents) {
    // contents()->DidBecomeSelected();
  }

  // Sets the id of the specified contents.
  void SetID(TabContents* contents, int id) {
    GetIDAccessor()->SetProperty(contents->property_bag(), id);
  }

  // Returns the id of the specified contents.
  int GetID(TabContents* contents) {
    return *GetIDAccessor()->GetProperty(contents->property_bag());
  }

  // Returns the state of the given tab strip as a string. The state consists
  // of the ID of each tab contents followed by a 'p' if pinned. For example,
  // if the model consists of two tabs with ids 2 and 1, with the first
  // tab pinned, this returns "2p 1".
  std::string GetPinnedState(const TabStripModel& model) {
    std::string actual;
    for (int i = 0; i < model.count(); ++i) {
      if (i > 0)
        actual += " ";

      actual +=
          base::IntToString(GetID(model.GetTabContentsAt(i)->tab_contents()));

      if (model.IsAppTab(i))
        actual += "a";

      if (model.IsTabPinned(i))
        actual += "p";
    }
    return actual;
  }

  std::string GetIndicesClosedByCommandAsString(
      const TabStripModel& model,
      int index,
      TabStripModel::ContextMenuCommand id) const {
    std::vector<int> indices = model.GetIndicesClosedByCommand(index, id);
    std::string result;
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i != 0)
        result += " ";
      result += base::IntToString(indices[i]);
    }
    return result;
  }

  void PrepareTabstripForSelectionTest(TabStripModel* model,
                                       int tab_count,
                                       int pinned_count,
                                       const std::string& selected_tabs) {
    for (int i = 0; i < tab_count; ++i) {
      TabContentsWrapper* contents = CreateTabContents();
      SetID(contents->tab_contents(), i);
      model->AppendTabContents(contents, true);
    }
    for (int i = 0; i < pinned_count; ++i)
      model->SetTabPinned(i, true);

    TabStripSelectionModel selection_model;
    std::vector<std::string> selection;
    base::SplitStringAlongWhitespace(selected_tabs, &selection);
    for (size_t i = 0; i < selection.size(); ++i) {
      int value;
      ASSERT_TRUE(base::StringToInt(selection[i], &value));
      selection_model.AddIndexToSelection(value);
    }
    selection_model.set_active(selection_model.selected_indices()[0]);
    model->SetSelectionFromModel(selection_model);
  }

 private:
  PropertyAccessor<int>* GetIDAccessor() {
    static PropertyAccessor<int> accessor;
    return &accessor;
  }

  content::TestBrowserThread browser_thread_;

  std::wstring test_dir_;
  std::wstring profile_path_;
  std::map<TabContents*, int> foo_;
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver() : empty_(true),
                                log_tab_selection_changed_(false),
                                model_(NULL) {}
  explicit MockTabStripModelObserver(TabStripModel* model)
      : empty_(true),
        log_tab_selection_changed_(false),
        model_(model) {}
  virtual ~MockTabStripModelObserver() {
    STLDeleteContainerPointers(states_.begin(), states_.end());
  }

  enum TabStripModelObserverAction {
    INSERT,
    CLOSE,
    DETACH,
    ACTIVATE,
    SELECT,
    MOVE,
    CHANGE,
    PINNED,
    REPLACED
  };

  struct State {
    State(TabContentsWrapper* a_dst_contents,
          int a_dst_index,
          TabStripModelObserverAction a_action)
        : src_contents(NULL),
          dst_contents(a_dst_contents),
          src_index(-1),
          dst_index(a_dst_index),
          user_gesture(false),
          foreground(false),
          action(a_action) {
    }

    TabContentsWrapper* src_contents;
    TabContentsWrapper* dst_contents;
    int src_index;
    int dst_index;
    bool user_gesture;
    bool foreground;
    TabStripModelObserverAction action;
  };

  int GetStateCount() const {
    return static_cast<int>(states_.size());
  }

  State* GetStateAt(int index) const {
    DCHECK(index >= 0 && index < GetStateCount());
    return states_.at(index);
  }

  bool StateEquals(int index, const State& state) {
    State* s = GetStateAt(index);
    EXPECT_EQ(state.src_contents, s->src_contents);
    EXPECT_EQ(state.dst_contents, s->dst_contents);
    EXPECT_EQ(state.src_index, s->src_index);
    EXPECT_EQ(state.dst_index, s->dst_index);
    EXPECT_EQ(state.user_gesture, s->user_gesture);
    EXPECT_EQ(state.foreground, s->foreground);
    EXPECT_EQ(state.action, s->action);
    return (s->src_contents == state.src_contents &&
            s->dst_contents == state.dst_contents &&
            s->src_index == state.src_index &&
            s->dst_index == state.dst_index &&
            s->user_gesture == state.user_gesture &&
            s->foreground == state.foreground &&
            s->action == state.action);
  }

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground) {
    empty_ = false;
    State* s = new State(contents, index, INSERT);
    s->foreground = foreground;
    states_.push_back(s);
  }
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) {
    State* s = new State(new_contents, index, ACTIVATE);
    s->src_contents = old_contents;
    s->user_gesture = user_gesture;
    states_.push_back(s);
  }
  virtual void TabSelectionChanged(TabStripModel* tab_strip_model,
                                   const TabStripSelectionModel& old_model) {
    if (!log_tab_selection_changed())
      return;

    State* s = new State(model()->GetActiveTabContents(),
                         model()->active_index(),
                         SELECT);
    s->src_contents = model()->GetTabContentsAt(old_model.active());
    s->src_index = old_model.active();
    states_.push_back(s);
  }
  virtual void TabMoved(
      TabContentsWrapper* contents, int from_index, int to_index) {
    State* s = new State(contents, to_index, MOVE);
    s->src_index = from_index;
    states_.push_back(s);
  }

  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) {
    states_.push_back(new State(contents, index, CLOSE));
  }
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) {
    states_.push_back(new State(contents, index, DETACH));
  }
  virtual void TabChangedAt(TabContentsWrapper* contents, int index,
                            TabChangeType change_type) {
    states_.push_back(new State(contents, index, CHANGE));
  }
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) {
    State* s = new State(new_contents, index, REPLACED);
    s ->src_contents = old_contents;
    states_.push_back(s);
  }
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents, int index) {
    states_.push_back(new State(contents, index, PINNED));
  }
  virtual void TabStripEmpty() {
    empty_ = true;
  }

  void ClearStates() {
    STLDeleteContainerPointers(states_.begin(), states_.end());
    states_.clear();
  }

  void set_log_tab_selection_changed(bool flag) {
    log_tab_selection_changed_ = flag;
  }

  bool empty() const { return empty_; }
  bool log_tab_selection_changed() const { return log_tab_selection_changed_; }
  TabStripModel* model() { return model_; }

 private:
  std::vector<State*> states_;

  bool empty_;
  // TODO(dpapad): Remove this variable and update TestBasicAPI so that it takes
  // into account TabSelectionChanged notifications.
  bool log_tab_selection_changed_;
  TabStripModel* model_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

TEST_F(TabStripModelTest, TestBasicAPI) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  TabContentsWrapper* contents1 = CreateTabContents();

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Test AppendTabContents, ContainsIndex
  {
    EXPECT_FALSE(tabstrip.ContainsIndex(0));
    tabstrip.AppendTabContents(contents1, true);
    EXPECT_TRUE(tabstrip.ContainsIndex(0));
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(2, observer.GetStateCount());
    State s1(contents1, 0, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents1, 0, MockTabStripModelObserver::ACTIVATE);
    s2.src_contents = NULL;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    observer.ClearStates();
  }

  // Test InsertTabContentsAt, foreground tab.
  TabContentsWrapper* contents2 = CreateTabContents();
  {
    tabstrip.InsertTabContentsAt(1, contents2, TabStripModel::ADD_ACTIVE);

    EXPECT_EQ(2, tabstrip.count());
    EXPECT_EQ(2, observer.GetStateCount());
    State s1(contents2, 1, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s2.src_contents = contents1;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    observer.ClearStates();
  }

  // Test InsertTabContentsAt, background tab.
  TabContentsWrapper* contents3 = CreateTabContents();
  {
    tabstrip.InsertTabContentsAt(2, contents3, TabStripModel::ADD_NONE);

    EXPECT_EQ(3, tabstrip.count());
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::INSERT);
    s1.foreground = false;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test ActivateTabAt
  {
    tabstrip.ActivateTabAt(2, true);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::ACTIVATE);
    s1.src_contents = contents2;
    s1.user_gesture = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test DetachTabContentsAt
  {
    // Detach
    TabContentsWrapper* detached = tabstrip.DetachTabContentsAt(2);
    // ... and append again because we want this for later.
    tabstrip.AppendTabContents(detached, true);
    EXPECT_EQ(4, observer.GetStateCount());
    State s1(detached, 2, MockTabStripModelObserver::DETACH);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s2.src_contents = contents3;
    s2.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(detached, 2, MockTabStripModelObserver::INSERT);
    s3.foreground = true;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    State s4(detached, 2, MockTabStripModelObserver::ACTIVATE);
    s4.src_contents = contents2;
    s4.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(3, s4));
    observer.ClearStates();
  }

  // Test CloseTabContentsAt
  {
    // Let's test nothing happens when the delegate veto the close.
    delegate.set_can_close(false);
    EXPECT_FALSE(tabstrip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE));
    EXPECT_EQ(3, tabstrip.count());
    EXPECT_EQ(0, observer.GetStateCount());

    // Now let's close for real.
    delegate.set_can_close(true);
    EXPECT_TRUE(tabstrip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE));
    EXPECT_EQ(2, tabstrip.count());

    EXPECT_EQ(3, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::CLOSE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents3, 2, MockTabStripModelObserver::DETACH);
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s3.src_contents = contents3;
    s3.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    observer.ClearStates();
  }

  // Test MoveTabContentsAt, select_after_move == true
  {
    tabstrip.MoveTabContentsAt(1, 0, true);

    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents2, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    EXPECT_EQ(0, tabstrip.active_index());
    observer.ClearStates();
  }

  // Test MoveTabContentsAt, select_after_move == false
  {
    tabstrip.MoveTabContentsAt(1, 0, false);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents1, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    EXPECT_EQ(1, tabstrip.active_index());

    tabstrip.MoveTabContentsAt(0, 1, false);
    observer.ClearStates();
  }

  // Test Getters
  {
    EXPECT_EQ(contents2, tabstrip.GetActiveTabContents());
    EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(0));
    EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(1));
    EXPECT_EQ(0, tabstrip.GetIndexOfTabContents(contents2));
    EXPECT_EQ(1, tabstrip.GetIndexOfTabContents(contents1));
    EXPECT_EQ(0, tabstrip.GetIndexOfController(&contents2->controller()));
    EXPECT_EQ(1, tabstrip.GetIndexOfController(&contents1->controller()));
  }

  // Test UpdateTabContentsStateAt
  {
    tabstrip.UpdateTabContentsStateAt(0, TabStripModelObserver::ALL);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents2, 0, MockTabStripModelObserver::CHANGE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test SelectNextTab, SelectPreviousTab, SelectLastTab
  {
    // Make sure the second of the two tabs is selected first...
    tabstrip.ActivateTabAt(1, true);
    tabstrip.SelectPreviousTab();
    EXPECT_EQ(0, tabstrip.active_index());
    tabstrip.SelectLastTab();
    EXPECT_EQ(1, tabstrip.active_index());
    tabstrip.SelectNextTab();
    EXPECT_EQ(0, tabstrip.active_index());
  }

  // Test CloseSelectedTabs
  {
    tabstrip.CloseSelectedTabs();
    // |CloseSelectedTabs| calls CloseTabContentsAt, we already tested that, now
    // just verify that the count and selected index have changed
    // appropriately...
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(0, tabstrip.active_index());
  }

  tabstrip.CloseAllTabs();
  // TabStripModel should now be empty.
  EXPECT_TRUE(tabstrip.empty());

  // Opener methods are tested below...

  tabstrip.RemoveObserver(&observer);
}

TEST_F(TabStripModelTest, TestBasicOpenerAPI) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // This is a basic test of opener functionality. opener_contents is created
  // as the first tab in the strip and then we create 5 other tabs in the
  // background with opener_contents set as their opener.

  TabContentsWrapper* opener_contents = CreateTabContents();
  NavigationController* opener = &opener_contents->controller();
  tabstrip.AppendTabContents(opener_contents, true);
  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();
  TabContentsWrapper* contents4 = CreateTabContents();
  TabContentsWrapper* contents5 = CreateTabContents();

  // We use |InsertTabContentsAt| here instead of AppendTabContents so that
  // openership relationships are preserved.
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents1,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents2,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents3,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents4,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents5,
                               TabStripModel::ADD_INHERIT_GROUP);

  // All the tabs should have the same opener.
  for (int i = 1; i < tabstrip.count(); ++i)
    EXPECT_EQ(opener, tabstrip.GetOpenerOfTabContentsAt(i));

  // If there is a next adjacent item, then the index should be of that item.
  EXPECT_EQ(2, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 1, false));
  // If the last tab in the group is closed, the preceding tab in the same
  // group should be selected.
  EXPECT_EQ(4, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 5, false));

  // Tests the method that finds the last tab opened by the same opener in the
  // strip (this is the insertion index for the next background tab for the
  // specified opener).
  EXPECT_EQ(5, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  // For a tab that has opened no other tabs, the return value should always be
  // -1...
  NavigationController* o1 = &contents1->controller();
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(o1, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastTabContentsOpenedBy(o1, 3));

  // ForgetAllOpeners should destroy all opener relationships.
  tabstrip.ForgetAllOpeners();
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 1, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 5, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

static int GetInsertionIndex(TabStripModel* tabstrip,
                             TabContentsWrapper* contents) {
  return tabstrip->order_controller()->DetermineInsertionIndex(
      contents, content::PAGE_TRANSITION_LINK, false);
}

static void InsertTabContentses(TabStripModel* tabstrip,
                                TabContentsWrapper* contents1,
                                TabContentsWrapper* contents2,
                                TabContentsWrapper* contents3) {
  tabstrip->InsertTabContentsAt(GetInsertionIndex(tabstrip, contents1),
                                contents1, TabStripModel::ADD_INHERIT_GROUP);
  tabstrip->InsertTabContentsAt(GetInsertionIndex(tabstrip, contents2),
                                contents2, TabStripModel::ADD_INHERIT_GROUP);
  tabstrip->InsertTabContentsAt(GetInsertionIndex(tabstrip, contents3),
                                contents3, TabStripModel::ADD_INHERIT_GROUP);
}

// Tests opening background tabs.
TEST_F(TabStripModelTest, TestLTRInsertionOptions) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  TabContentsWrapper* opener_contents = CreateTabContents();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  // Test LTR
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(contents3, tabstrip.GetTabContentsAt(3));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests inserting tabs with InsertAfter set to false.
TEST_F(TabStripModelTest, InsertBefore) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.SetInsertionPolicy(TabStripModel::INSERT_BEFORE);
  EXPECT_TRUE(tabstrip.empty());

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  InsertTabContentses(&tabstrip, contents1, contents2, contents3);

  // The order should be reversed.
  EXPECT_EQ(contents3, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(2));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests opening background tabs with InsertAfter set to false.
TEST_F(TabStripModelTest, InsertBeforeOpeners) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.SetInsertionPolicy(TabStripModel::INSERT_BEFORE);
  EXPECT_TRUE(tabstrip.empty());
  TabContentsWrapper* opener_contents = CreateTabContents();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  InsertTabContentses(&tabstrip, contents1, contents2, contents3);

  // The order should be reversed.
  EXPECT_EQ(contents3, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(2));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// This test constructs a tabstrip, and then simulates loading several tabs in
// the background from link clicks on the first tab. Then it simulates opening
// a new tab from the first tab in the foreground via a link click, verifies
// that this tab is opened adjacent to the opener, then closes it.
// Finally it tests that a tab opened for some non-link purpose openes at the
// end of the strip, not bundled to any existing context.
TEST_F(TabStripModelTest, TestInsertionIndexDetermination) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  TabContentsWrapper* opener_contents = CreateTabContents();
  NavigationController* opener = &opener_contents->controller();
  tabstrip.AppendTabContents(opener_contents, true);

  // Open some other random unrelated tab in the background to monkey with our
  // insertion index.
  TabContentsWrapper* other_contents = CreateTabContents();
  tabstrip.AppendTabContents(other_contents, false);

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  // Start by testing LTR
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(opener_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(contents3, tabstrip.GetTabContentsAt(3));
  EXPECT_EQ(other_contents, tabstrip.GetTabContentsAt(4));

  // The opener API should work...
  EXPECT_EQ(3, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 2, false));
  EXPECT_EQ(2, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(3, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  // Now open a foreground tab from a link. It should be opened adjacent to the
  // opener tab.
  TabContentsWrapper* fg_link_contents = CreateTabContents();
  int insert_index = tabstrip.order_controller()->DetermineInsertionIndex(
      fg_link_contents, content::PAGE_TRANSITION_LINK, true);
  EXPECT_EQ(1, insert_index);
  tabstrip.InsertTabContentsAt(insert_index, fg_link_contents,
                               TabStripModel::ADD_ACTIVE |
                               TabStripModel::ADD_INHERIT_GROUP);
  EXPECT_EQ(1, tabstrip.active_index());
  EXPECT_EQ(fg_link_contents, tabstrip.GetActiveTabContents());

  // Now close this contents. The selection should move to the opener contents.
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(0, tabstrip.active_index());

  // Now open a new empty tab. It should open at the end of the strip.
  TabContentsWrapper* fg_nonlink_contents = CreateTabContents();
  insert_index = tabstrip.order_controller()->DetermineInsertionIndex(
      fg_nonlink_contents, content::PAGE_TRANSITION_AUTO_BOOKMARK, true);
  EXPECT_EQ(tabstrip.count(), insert_index);
  // We break the opener relationship...
  tabstrip.InsertTabContentsAt(insert_index, fg_nonlink_contents,
                               TabStripModel::ADD_NONE);
  // Now select it, so that user_gesture == true causes the opener relationship
  // to be forgotten...
  tabstrip.ActivateTabAt(tabstrip.count() - 1, true);
  EXPECT_EQ(tabstrip.count() - 1, tabstrip.active_index());
  EXPECT_EQ(fg_nonlink_contents, tabstrip.GetActiveTabContents());

  // Verify that all opener relationships are forgotten.
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 2, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If a tab is in the background when it is closed, the selection does not
// change.
// If a tab is in the foreground (selected),
//   If that tab does not have an opener, selection shifts to the right.
//   If the tab has an opener,
//     The next tab (scanning LTR) in the entire strip that has the same opener
//     is selected
//     If there are no other tabs that have the same opener,
//       The opener is selected
//
TEST_F(TabStripModelTest, TestSelectOnClose) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  TabContentsWrapper* opener_contents = CreateTabContents();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  // Note that we use Detach instead of Close throughout this test to avoid
  // having to keep reconstructing these TabContentses.

  // First test that closing tabs that are in the background doesn't adjust the
  // current selection.
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(0, tabstrip.active_index());

  for (int i = tabstrip.count() - 1; i >= 1; --i)
    tabstrip.DetachTabContentsAt(i);

  // Now test that when a tab doesn't have an opener, selection shifts to the
  // right when the tab is closed.
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tabstrip.ActivateTabAt(1, true);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(0, tabstrip.active_index());

  for (int i = tabstrip.count() - 1; i >= 1; --i)
    tabstrip.DetachTabContentsAt(i);

  // Now test that when a tab does have an opener, it selects the next tab
  // opened by the same opener scanning LTR when it is closed.
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());
  tabstrip.ActivateTabAt(2, false);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.CloseTabContentsAt(1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());
  // Finally test that when a tab has no "siblings" that the opener is
  // selected.
  TabContentsWrapper* other_contents = CreateTabContents();
  tabstrip.InsertTabContentsAt(1, other_contents, TabStripModel::ADD_NONE);
  EXPECT_EQ(2, tabstrip.count());
  TabContentsWrapper* opened_contents = CreateTabContents();
  tabstrip.InsertTabContentsAt(2, opened_contents,
                               TabStripModel::ADD_ACTIVE |
                               TabStripModel::ADD_INHERIT_GROUP);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTab.
TEST_F(TabStripModelTest, CommandCloseTab) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Make sure can_close is honored.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 1, 0, "0"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTab));
  delegate.set_can_close(false);
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                   0, TabStripModel::CommandCloseTab));
  delegate.set_can_close(true);
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  ASSERT_TRUE(tabstrip.empty());

  // Make sure close on a tab that is selected effects all the selected tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  // Should have closed tabs 0 and 1.
  EXPECT_EQ("2", GetPinnedState(tabstrip));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select two tabs and make close on a tab that isn't selected doesn't effect
  // selected tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  2, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  // Should have closed tab 2.
  EXPECT_EQ("0 1", GetPinnedState(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Tests with 3 tabs, one pinned, two tab selected, one of which is pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  // Should have closed tab 2.
  EXPECT_EQ("2", GetPinnedState(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTabs.
TEST_F(TabStripModelTest, CommandCloseOtherTabs) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs, select two tabs, CommandCloseOtherTabs should be enabled
  // and close two tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ("0 1", GetPinnedState(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select two tabs, CommandCloseOtherTabs should be enabled and invoking it
  // with a non-selected index should close the two other tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  2, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ("0 1", GetPinnedState(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select all, CommandCloseOtherTabs should not be enabled.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1 2"));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
                  2, TabStripModel::CommandCloseOtherTabs));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Three tabs, pin one, select the two non-pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "1 2"));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
                  1, TabStripModel::CommandCloseOtherTabs));
  // If we don't pass in the pinned index, the command should be enabled.
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // 3 tabs, one pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  1, TabStripModel::CommandCloseOtherTabs));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(1, TabStripModel::CommandCloseOtherTabs);
  // The pinned tab shouldn't be closed.
  EXPECT_EQ("0p 1", GetPinnedState(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTabsToRight.
TEST_F(TabStripModelTest, CommandCloseTabsToRight) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs, select last two tabs, CommandCloseTabsToRight should
  // only be enabled for the first tab.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "1 2"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTabsToRight));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
                   1, TabStripModel::CommandCloseTabsToRight));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
                   2, TabStripModel::CommandCloseTabsToRight));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsToRight);
  EXPECT_EQ("0", GetPinnedState(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandTogglePinned.
TEST_F(TabStripModelTest, CommandTogglePinned) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs with one pinned, pin the first two.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandTogglePinned));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  1, TabStripModel::CommandTogglePinned));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  2, TabStripModel::CommandTogglePinned));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("0p 1p 2", GetPinnedState(tabstrip));

  // Execute CommandTogglePinned again, this should unpin.
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("0 1 2", GetPinnedState(tabstrip));

  // Pin the last.
  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("2p 0 1", GetPinnedState(tabstrip));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests the following context menu commands:
//  - Close Tab
//  - Close Other Tabs
//  - Close Tabs To Right
TEST_F(TabStripModelTest, TestContextMenuCloseCommands) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  TabContentsWrapper* opener_contents = CreateTabContents();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  EXPECT_EQ(3, tabstrip.count());

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsToRight);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(opener_contents, tabstrip.GetActiveTabContents());

  TabContentsWrapper* dummy_contents = CreateTabContents();
  tabstrip.AppendTabContents(dummy_contents, false);

  contents1 = CreateTabContents();
  contents2 = CreateTabContents();
  contents3 = CreateTabContents();
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(5, tabstrip.count());

  int dummy_index = tabstrip.count() - 1;
  tabstrip.ActivateTabAt(dummy_index, true);
  EXPECT_EQ(dummy_contents, tabstrip.GetActiveTabContents());

  tabstrip.ExecuteContextMenuCommand(dummy_index,
                                     TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(dummy_contents, tabstrip.GetActiveTabContents());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests GetIndicesClosedByCommand.
TEST_F(TabStripModelTest, GetIndicesClosedByCommand) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();
  TabContentsWrapper* contents4 = CreateTabContents();
  TabContentsWrapper* contents5 = CreateTabContents();

  tabstrip.AppendTabContents(contents1, true);
  tabstrip.AppendTabContents(contents2, true);
  tabstrip.AppendTabContents(contents3, true);
  tabstrip.AppendTabContents(contents4, true);
  tabstrip.AppendTabContents(contents5, true);

  EXPECT_EQ("4 3 2 1", GetIndicesClosedByCommandAsString(
                tabstrip, 0, TabStripModel::CommandCloseTabsToRight));
  EXPECT_EQ("4 3 2", GetIndicesClosedByCommandAsString(
                tabstrip, 1, TabStripModel::CommandCloseTabsToRight));

  EXPECT_EQ("4 3 2 1", GetIndicesClosedByCommandAsString(
                tabstrip, 0, TabStripModel::CommandCloseOtherTabs));
  EXPECT_EQ("4 3 2 0", GetIndicesClosedByCommandAsString(
                tabstrip, 1, TabStripModel::CommandCloseOtherTabs));

  // Pin the first two tabs. Pinned tabs shouldn't be closed by the close other
  // commands.
  tabstrip.SetTabPinned(0, true);
  tabstrip.SetTabPinned(1, true);

  EXPECT_EQ("4 3 2", GetIndicesClosedByCommandAsString(
                tabstrip, 0, TabStripModel::CommandCloseTabsToRight));
  EXPECT_EQ("4 3", GetIndicesClosedByCommandAsString(
                tabstrip, 2, TabStripModel::CommandCloseTabsToRight));

  EXPECT_EQ("4 3 2", GetIndicesClosedByCommandAsString(
                tabstrip, 0, TabStripModel::CommandCloseOtherTabs));
  EXPECT_EQ("4 3", GetIndicesClosedByCommandAsString(
                tabstrip, 2, TabStripModel::CommandCloseOtherTabs));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not TabContentses are inserted in the correct position
// using this "smart" function with a simulated middle click action on a series
// of links on the home page.
TEST_F(TabStripModelTest, AddTabContents_MiddleClickLinksAndClose) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContentsWrapper* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  TabContentsWrapper* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  TabContentsWrapper* middle_click_contents1 = CreateTabContents();
  tabstrip.AddTabContents(
      middle_click_contents1, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  TabContentsWrapper* middle_click_contents2 = CreateTabContents();
  tabstrip.AddTabContents(
      middle_click_contents2, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  TabContentsWrapper* middle_click_contents3 = CreateTabContents();
  tabstrip.AddTabContents(
      middle_click_contents3, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);

  EXPECT_EQ(5, tabstrip.count());

  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(middle_click_contents1, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(middle_click_contents2, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(middle_click_contents3, tabstrip.GetTabContentsAt(3));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(4));

  // Now simulate seleting a tab in the middle of the group of tabs opened from
  // the home page and start closing them. Each TabContents in the group should
  // be closed, right to left. This test is constructed to start at the middle
  // TabContents in the group to make sure the cursor wraps around to the first
  // TabContents in the group before closing the opener or any other
  // TabContents.
  tabstrip.ActivateTabAt(2, true);
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents3, tabstrip.GetActiveTabContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents1, tabstrip.GetActiveTabContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(homepage_contents, tabstrip.GetActiveTabContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(typed_page_contents, tabstrip.GetActiveTabContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not a TabContents created by a left click on a link that
// opens a new tab is inserted correctly adjacent to the tab that spawned it.
TEST_F(TabStripModelTest, AddTabContents_LeftClickPopup) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContentsWrapper* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  TabContentsWrapper* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a tab by simulating a left click on a link that opens in a new tab.
  TabContentsWrapper* left_click_contents = CreateTabContents();
  tabstrip.AddTabContents(left_click_contents, -1,
                          content::PAGE_TRANSITION_LINK,
                          TabStripModel::ADD_ACTIVE);

  // Verify the state meets our expectations.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(left_click_contents, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(2));

  // The newly created tab should be selected.
  EXPECT_EQ(left_click_contents, tabstrip.GetActiveTabContents());

  // After closing the selected tab, the selection should move to the left, to
  // the opener.
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(homepage_contents, tabstrip.GetActiveTabContents());

  EXPECT_EQ(2, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not new tabs that should split context (typed pages,
// generated urls, also blank tabs) open at the end of the tabstrip instead of
// in the middle.
TEST_F(TabStripModelTest, AddTabContents_CreateNewBlankTab) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContentsWrapper* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  TabContentsWrapper* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a new blank tab in the foreground.
  TabContentsWrapper* new_blank_contents = CreateTabContents();
  tabstrip.AddTabContents(new_blank_contents, -1,
                          content::PAGE_TRANSITION_TYPED,
                          TabStripModel::ADD_ACTIVE);

  // Verify the state of the tabstrip.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(new_blank_contents, tabstrip.GetTabContentsAt(2));

  // Now open a couple more blank tabs in the background.
  TabContentsWrapper* background_blank_contents1 = CreateTabContents();
  tabstrip.AddTabContents(
      background_blank_contents1, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_NONE);
  TabContentsWrapper* background_blank_contents2 = CreateTabContents();
  tabstrip.AddTabContents(
      background_blank_contents2, -1, content::PAGE_TRANSITION_GENERATED,
      TabStripModel::ADD_NONE);
  EXPECT_EQ(5, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(new_blank_contents, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(background_blank_contents1, tabstrip.GetTabContentsAt(3));
  EXPECT_EQ(background_blank_contents2, tabstrip.GetTabContentsAt(4));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether opener state is correctly forgotten when the user switches
// context.
TEST_F(TabStripModelTest, AddTabContents_ForgetOpeners) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContentsWrapper* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  TabContentsWrapper* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  TabContentsWrapper* middle_click_contents1 = CreateTabContents();
  tabstrip.AddTabContents(
      middle_click_contents1, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  TabContentsWrapper* middle_click_contents2 = CreateTabContents();
  tabstrip.AddTabContents(
      middle_click_contents2, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  TabContentsWrapper* middle_click_contents3 = CreateTabContents();
  tabstrip.AddTabContents(
      middle_click_contents3, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);

  // Break out of the context by selecting a tab in a different context.
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(4));
  tabstrip.SelectLastTab();
  EXPECT_EQ(typed_page_contents, tabstrip.GetActiveTabContents());

  // Step back into the context by selecting a tab inside it.
  tabstrip.ActivateTabAt(2, true);
  EXPECT_EQ(middle_click_contents2, tabstrip.GetActiveTabContents());

  // Now test that closing tabs selects to the right until there are no more,
  // then to the left, as if there were no context (context has been
  // successfully forgotten).
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents3, tabstrip.GetActiveTabContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(typed_page_contents, tabstrip.GetActiveTabContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents1, tabstrip.GetActiveTabContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(homepage_contents, tabstrip.GetActiveTabContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Added for http://b/issue?id=958960
TEST_F(TabStripModelTest, AppendContentsReselectionTest) {
  TabContents* fake_destinations_tab =
      new TabContents(profile(), NULL, 0, NULL, NULL);
  TabContentsWrapper wrapper(fake_destinations_tab);
  TabStripDummyDelegate delegate(&wrapper);
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContentsWrapper* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  TabContentsWrapper* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_NONE);

  // The selected tab should still be the first.
  EXPECT_EQ(0, tabstrip.active_index());

  // Now simulate a link click that opens a new tab (by virtue of target=_blank)
  // and make sure the right tab gets selected when the new tab is closed.
  TabContentsWrapper* target_blank_contents = CreateTabContents();
  tabstrip.AppendTabContents(target_blank_contents, true);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  // clean up after ourselves
  tabstrip.CloseAllTabs();
}

// Added for http://b/issue?id=1027661
TEST_F(TabStripModelTest, ReselectionConsidersChildrenTest) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());

  // Open page A
  TabContentsWrapper* page_a_contents = CreateTabContents();
  strip.AddTabContents(
      page_a_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Simulate middle click to open page A.A and A.B
  TabContentsWrapper* page_a_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_a_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  TabContentsWrapper* page_a_b_contents = CreateTabContents();
  strip.AddTabContents(page_a_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Select page A.A
  strip.ActivateTabAt(1, true);
  EXPECT_EQ(page_a_a_contents, strip.GetActiveTabContents());

  // Simulate a middle click to open page A.A.A
  TabContentsWrapper* page_a_a_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_a_a_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  EXPECT_EQ(page_a_a_a_contents, strip.GetTabContentsAt(2));

  // Close page A.A
  strip.CloseTabContentsAt(strip.active_index(), TabStripModel::CLOSE_NONE);

  // Page A.A.A should be selected, NOT A.B
  EXPECT_EQ(page_a_a_a_contents, strip.GetActiveTabContents());

  // Close page A.A.A
  strip.CloseTabContentsAt(strip.active_index(), TabStripModel::CLOSE_NONE);

  // Page A.B should be selected
  EXPECT_EQ(page_a_b_contents, strip.GetActiveTabContents());

  // Close page A.B
  strip.CloseTabContentsAt(strip.active_index(), TabStripModel::CLOSE_NONE);

  // Page A should be selected
  EXPECT_EQ(page_a_contents, strip.GetActiveTabContents());

  // Clean up.
  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, AddTabContents_NewTabAtEndOfStripInheritsGroup) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());

  // Open page A
  TabContentsWrapper* page_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_contents, -1, content::PAGE_TRANSITION_START_PAGE,
                       TabStripModel::ADD_ACTIVE);

  // Open pages B, C and D in the background from links on page A...
  TabContentsWrapper* page_b_contents = CreateTabContents();
  TabContentsWrapper* page_c_contents = CreateTabContents();
  TabContentsWrapper* page_d_contents = CreateTabContents();
  strip.AddTabContents(page_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddTabContents(page_c_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddTabContents(page_d_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Switch to page B's tab.
  strip.ActivateTabAt(1, true);

  // Open a New Tab at the end of the strip (simulate Ctrl+T)
  TabContentsWrapper* new_tab_contents = CreateTabContents();
  strip.AddTabContents(new_tab_contents, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfTabContents(new_tab_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the New Tab that was just opened. We should be returned to page B's
  // Tab...
  strip.CloseTabContentsAt(4, TabStripModel::CLOSE_NONE);

  EXPECT_EQ(1, strip.active_index());

  // Open a non-New Tab tab at the end of the strip, with a TYPED transition.
  // This is like typing a URL in the address bar and pressing Alt+Enter. The
  // behavior should be the same as above.
  TabContentsWrapper* page_e_contents = CreateTabContents();
  strip.AddTabContents(page_e_contents, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfTabContents(page_e_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the Tab. Selection should shift back to page B's Tab.
  strip.CloseTabContentsAt(4, TabStripModel::CLOSE_NONE);

  EXPECT_EQ(1, strip.active_index());

  // Open a non-New Tab tab at the end of the strip, with some other
  // transition. This is like right clicking on a bookmark and choosing "Open
  // in New Tab". No opener relationship should be preserved between this Tab
  // and the one that was active when the gesture was performed.
  TabContentsWrapper* page_f_contents = CreateTabContents();
  strip.AddTabContents(page_f_contents, -1,
                       content::PAGE_TRANSITION_AUTO_BOOKMARK,
                       TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfTabContents(page_f_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the Tab. The next-adjacent should be selected.
  strip.CloseTabContentsAt(4, TabStripModel::CLOSE_NONE);

  EXPECT_EQ(3, strip.active_index());

  // Clean up.
  strip.CloseAllTabs();
}

// A test of navigations in a tab that is part of a group of opened from some
// parent tab. If the navigations are link clicks, the group relationship of
// the tab to its parent are preserved. If they are of any other type, they are
// not preserved.
TEST_F(TabStripModelTest, NavigationForgetsOpeners) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());

  // Open page A
  TabContentsWrapper* page_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_contents, -1, content::PAGE_TRANSITION_START_PAGE,
                       TabStripModel::ADD_ACTIVE);

  // Open pages B, C and D in the background from links on page A...
  TabContentsWrapper* page_b_contents = CreateTabContents();
  TabContentsWrapper* page_c_contents = CreateTabContents();
  TabContentsWrapper* page_d_contents = CreateTabContents();
  strip.AddTabContents(page_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddTabContents(page_c_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddTabContents(page_d_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Open page E in a different opener group from page A.
  TabContentsWrapper* page_e_contents = CreateTabContents();
  strip.AddTabContents(page_e_contents, -1, content::PAGE_TRANSITION_START_PAGE,
                       TabStripModel::ADD_NONE);

  // Tell the TabStripModel that we are navigating page D via a link click.
  strip.ActivateTabAt(3, true);
  strip.TabNavigating(page_d_contents, content::PAGE_TRANSITION_LINK);

  // Close page D, page C should be selected. (part of same group).
  strip.CloseTabContentsAt(3, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(2, strip.active_index());

  // Tell the TabStripModel that we are navigating in page C via a bookmark.
  strip.TabNavigating(page_c_contents, content::PAGE_TRANSITION_AUTO_BOOKMARK);

  // Close page C, page E should be selected. (C is no longer part of the
  // A-B-C-D group, selection moves to the right).
  strip.CloseTabContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(page_e_contents, strip.GetTabContentsAt(strip.active_index()));

  strip.CloseAllTabs();
}

// A test that the forgetting behavior tested in NavigationForgetsOpeners above
// doesn't cause the opener relationship for a New Tab opened at the end of the
// TabStrip to be reset (Test 1 below), unless another any other tab is
// seelcted (Test 2 below).
TEST_F(TabStripModelTest, NavigationForgettingDoesntAffectNewTab) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());

  // Open a tab and several tabs from it, then select one of the tabs that was
  // opened.
  TabContentsWrapper* page_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_contents, -1, content::PAGE_TRANSITION_START_PAGE,
                       TabStripModel::ADD_ACTIVE);

  TabContentsWrapper* page_b_contents = CreateTabContents();
  TabContentsWrapper* page_c_contents = CreateTabContents();
  TabContentsWrapper* page_d_contents = CreateTabContents();
  strip.AddTabContents(page_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddTabContents(page_c_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddTabContents(page_d_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  strip.ActivateTabAt(2, true);

  // TEST 1: If the user is in a group of tabs and opens a new tab at the end
  // of the strip, closing that new tab will select the tab that they were
  // last on.

  // Now simulate opening a new tab at the end of the TabStrip.
  TabContentsWrapper* new_tab_contents1 = CreateTabContents();
  strip.AddTabContents(new_tab_contents1, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  // At this point, if we close this tab the last selected one should be
  // re-selected.
  strip.CloseTabContentsAt(strip.count() - 1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(page_c_contents, strip.GetTabContentsAt(strip.active_index()));

  // TEST 2: If the user is in a group of tabs and opens a new tab at the end
  // of the strip, selecting any other tab in the strip will cause that new
  // tab's opener relationship to be forgotten.

  // Open a new tab again.
  TabContentsWrapper* new_tab_contents2 = CreateTabContents();
  strip.AddTabContents(new_tab_contents2, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  // Now select the first tab.
  strip.ActivateTabAt(0, true);

  // Now select the last tab.
  strip.ActivateTabAt(strip.count() - 1, true);

  // Now close the last tab. The next adjacent should be selected.
  strip.CloseTabContentsAt(strip.count() - 1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(page_d_contents, strip.GetTabContentsAt(strip.active_index()));

  strip.CloseAllTabs();
}

// Tests that fast shutdown is attempted appropriately.
TEST_F(TabStripModelTest, FastShutdown) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  // Make sure fast shutdown is attempted when tabs that share a RPH are shut
  // down.
  {
    TabContentsWrapper* contents1 = CreateTabContents();
    TabContentsWrapper* contents2 =
        CreateTabContentsWithSharedRPH(contents1->tab_contents());

    SetID(contents1->tab_contents(), 1);
    SetID(contents2->tab_contents(), 2);

    tabstrip.AppendTabContents(contents1, true);
    tabstrip.AppendTabContents(contents2, true);

    // Turn on the fake unload listener so the tabs don't actually get shut
    // down when we call CloseAllTabs()---we need to be able to check that
    // fast shutdown was attempted.
    delegate.set_run_unload_listener(true);
    tabstrip.CloseAllTabs();
    // On a mock RPH this checks whether we *attempted* fast shutdown.
    // A real RPH would reject our attempt since there is an unload handler.
    EXPECT_TRUE(contents1->tab_contents()->
      GetRenderProcessHost()->FastShutdownStarted());
    EXPECT_EQ(2, tabstrip.count());

    delegate.set_run_unload_listener(false);
    tabstrip.CloseAllTabs();
    EXPECT_TRUE(tabstrip.empty());
  }

  // Make sure fast shutdown is not attempted when only some tabs that share a
  // RPH are shut down.
  {
    TabContentsWrapper* contents1 = CreateTabContents();
    TabContentsWrapper* contents2 =
        CreateTabContentsWithSharedRPH(contents1->tab_contents());

    SetID(contents1->tab_contents(), 1);
    SetID(contents2->tab_contents(), 2);

    tabstrip.AppendTabContents(contents1, true);
    tabstrip.AppendTabContents(contents2, true);

    tabstrip.CloseTabContentsAt(1, TabStripModel::CLOSE_NONE);
    EXPECT_FALSE(contents1->tab_contents()->
        GetRenderProcessHost()->FastShutdownStarted());
    EXPECT_EQ(1, tabstrip.count());

    tabstrip.CloseAllTabs();
    EXPECT_TRUE(tabstrip.empty());
  }
}

// Tests various permutations of apps.
TEST_F(TabStripModelTest, Apps) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  scoped_refptr<Extension> extension_app(new Extension(path,
                                                       Extension::INVALID));
  extension_app->launch_web_url_ = "http://www.google.com";
  TabContentsWrapper* contents1 = CreateTabContents();
  contents1->extension_tab_helper()->SetExtensionApp(extension_app);
  TabContentsWrapper* contents2 = CreateTabContents();
  contents2->extension_tab_helper()->SetExtensionApp(extension_app);
  TabContentsWrapper* contents3 = CreateTabContents();

  SetID(contents1->tab_contents(), 1);
  SetID(contents2->tab_contents(), 2);
  SetID(contents3->tab_contents(), 3);

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Initial state, tab3 only and selected.
  tabstrip.AppendTabContents(contents3, true);

  observer.ClearStates();

  // Attempt to insert tab1 (an app tab) at position 1. This isn't a legal
  // position and tab1 should end up at position 0.
  {
    tabstrip.InsertTabContentsAt(1, contents1, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 0, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1ap 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Insert tab 2 at position 1.
  {
    tabstrip.InsertTabContentsAt(1, contents2, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents2, 1, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1ap 2ap 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab 3 to position 0. This isn't legal and should be ignored.
  {
    tabstrip.MoveTabContentsAt(2, 0, false);

    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state didn't change.
    EXPECT_EQ("1ap 2ap 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab 0 to position 3. This isn't legal and should be ignored.
  {
    tabstrip.MoveTabContentsAt(0, 2, false);

    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state didn't change.
    EXPECT_EQ("1ap 2ap 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab 0 to position 1. This is a legal move.
  {
    tabstrip.MoveTabContentsAt(0, 1, false);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 1, MockTabStripModelObserver::MOVE);
    state.src_index = 0;
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state didn't change.
    EXPECT_EQ("2ap 1ap 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Remove tab3 and insert at position 0. It should be forced to position 2.
  {
    tabstrip.DetachTabContentsAt(2);
    observer.ClearStates();

    tabstrip.InsertTabContentsAt(0, contents3, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents3, 2, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state didn't change.
    EXPECT_EQ("2ap 1ap 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  tabstrip.CloseAllTabs();
}

// Tests various permutations of pinning tabs.
TEST_F(TabStripModelTest, Pinning) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();

  SetID(contents1->tab_contents(), 1);
  SetID(contents2->tab_contents(), 2);
  SetID(contents3->tab_contents(), 3);

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Initial state, three tabs, first selected.
  tabstrip.AppendTabContents(contents1, true);
  tabstrip.AppendTabContents(contents2, false);
  tabstrip.AppendTabContents(contents3, false);

  observer.ClearStates();

  // Pin the first tab, this shouldn't visually reorder anything.
  {
    tabstrip.SetTabPinned(0, true);

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 0, MockTabStripModelObserver::PINNED);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1p 2 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Unpin the first tab.
  {
    tabstrip.SetTabPinned(0, false);

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 0, MockTabStripModelObserver::PINNED);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1 2 3", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Pin the 3rd tab, which should move it to the front.
  {
    tabstrip.SetTabPinned(2, true);

    // The pinning should have resulted in a move and a pinned notification.
    ASSERT_EQ(2, observer.GetStateCount());
    State state(contents3, 0, MockTabStripModelObserver::MOVE);
    state.src_index = 2;
    EXPECT_TRUE(observer.StateEquals(0, state));

    state = State(contents3, 0, MockTabStripModelObserver::PINNED);
    EXPECT_TRUE(observer.StateEquals(1, state));

    // And verify the state.
    EXPECT_EQ("3p 1 2", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Pin the tab "1", which shouldn't move anything.
  {
    tabstrip.SetTabPinned(1, true);

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 1, MockTabStripModelObserver::PINNED);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("3p 1p 2", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab "2" to the front, it should be ignored.
  {
    tabstrip.MoveTabContentsAt(2, 0, false);

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state.
    EXPECT_EQ("3p 1p 2", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Unpin tab "3", which implicitly moves it to the end.
  {
    tabstrip.SetTabPinned(0, false);

    ASSERT_EQ(2, observer.GetStateCount());
    State state(contents3, 1, MockTabStripModelObserver::MOVE);
    state.src_index = 0;
    EXPECT_TRUE(observer.StateEquals(0, state));

    state = State(contents3, 1, MockTabStripModelObserver::PINNED);
    EXPECT_TRUE(observer.StateEquals(1, state));

    // And verify the state.
    EXPECT_EQ("1p 3 2", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Unpin tab "3", nothing should happen.
  {
    tabstrip.SetTabPinned(1, false);

    ASSERT_EQ(0, observer.GetStateCount());

    EXPECT_EQ("1p 3 2", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  // Pin "3" and "1".
  {
    tabstrip.SetTabPinned(0, true);
    tabstrip.SetTabPinned(1, true);

    EXPECT_EQ("1p 3p 2", GetPinnedState(tabstrip));

    observer.ClearStates();
  }

  TabContentsWrapper* contents4 = CreateTabContents();
  SetID(contents4->tab_contents(), 4);

  // Insert "4" between "1" and "3". As "1" and "4" are pinned, "4" should end
  // up after them.
  {
    tabstrip.InsertTabContentsAt(1, contents4, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents4, 2, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    EXPECT_EQ("1p 3p 4 2", GetPinnedState(tabstrip));
  }

  tabstrip.CloseAllTabs();
}

// Makes sure the TabStripModel calls the right observer methods during a
// replace.
TEST_F(TabStripModelTest, ReplaceSendsSelected) {
  typedef MockTabStripModelObserver::State State;

  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());

  TabContentsWrapper* first_contents = CreateTabContents();
  strip.AddTabContents(first_contents, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  MockTabStripModelObserver tabstrip_observer;
  strip.AddObserver(&tabstrip_observer);

  TabContentsWrapper* new_contents = CreateTabContents();
  delete strip.ReplaceTabContentsAt(0, new_contents);

  ASSERT_EQ(2, tabstrip_observer.GetStateCount());

  // First event should be for replaced.
  State state(new_contents, 0, MockTabStripModelObserver::REPLACED);
  state.src_contents = first_contents;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state));

  // And the second for selected.
  state = State(new_contents, 0, MockTabStripModelObserver::ACTIVATE);
  state.src_contents = first_contents;
  EXPECT_TRUE(tabstrip_observer.StateEquals(1, state));

  // Now add another tab and replace it, making sure we don't get a selected
  // event this time.
  TabContentsWrapper* third_contents = CreateTabContents();
  strip.AddTabContents(third_contents, 1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_NONE);

  tabstrip_observer.ClearStates();

  // And replace it.
  new_contents = CreateTabContents();
  delete strip.ReplaceTabContentsAt(1, new_contents);

  ASSERT_EQ(1, tabstrip_observer.GetStateCount());

  state = State(new_contents, 1, MockTabStripModelObserver::REPLACED);
  state.src_contents = third_contents;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state));

  strip.CloseAllTabs();
}

// Ensures discarding tabs leaves TabStripModel in a good state.
TEST_F(TabStripModelTest, DiscardTabContentsAt) {
  typedef MockTabStripModelObserver::State State;

  TabStripDummyDelegate delegate(NULL);
  TabStripModel tabstrip(&delegate, profile());

  // Fill it with some tabs.
  TabContentsWrapper* contents1 = CreateTabContents();
  tabstrip.AppendTabContents(contents1, true);
  TabContentsWrapper* contents2 = CreateTabContents();
  tabstrip.AppendTabContents(contents2, true);

  // Start watching for events after the appends to avoid observing state
  // transitions that aren't relevant to this test.
  MockTabStripModelObserver tabstrip_observer;
  tabstrip.AddObserver(&tabstrip_observer);

  // Discard one of the tabs.
  TabContentsWrapper* null_contents1 = tabstrip.DiscardTabContentsAt(0);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));
  ASSERT_EQ(null_contents1, tabstrip.GetTabContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetTabContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.GetStateCount());
  State state1(null_contents1, 0, MockTabStripModelObserver::REPLACED);
  state1.src_contents = contents1;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state1));
  tabstrip_observer.ClearStates();

  // Discard the same tab again.
  TabContentsWrapper* null_contents2 = tabstrip.DiscardTabContentsAt(0);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));
  ASSERT_EQ(null_contents2, tabstrip.GetTabContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetTabContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.GetStateCount());
  State state2(null_contents2, 0, MockTabStripModelObserver::REPLACED);
  state2.src_contents = null_contents1;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state2));
  tabstrip_observer.ClearStates();

  // Activating the tab should clear its discard state.
  tabstrip.ActivateTabAt(0, true /* user_gesture */);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_FALSE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));

  // Discarding the active tab should not crash.
  TabContentsWrapper* null_contents3 = tabstrip.DiscardTabContentsAt(0);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));
  ASSERT_EQ(null_contents3, tabstrip.GetTabContentsAt(0));

  tabstrip.CloseAllTabs();
}

// Makes sure TabStripModel handles the case of deleting a tab while removing
// another tab.
TEST_F(TabStripModelTest, DeleteFromDestroy) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());
  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  strip.AppendTabContents(contents1, true);
  strip.AppendTabContents(contents2, true);
  // DeleteTabContentsOnDestroyedObserver deletes contents1 when contents2 sends
  // out notification that it is being destroyed.
  DeleteTabContentsOnDestroyedObserver observer(contents2, contents1);
  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, MoveSelectedTabsTo) {
  struct TestData {
    // Number of tabs the tab strip should have.
    const int tab_count;

    // Number of pinned tabs.
    const int pinned_count;

    // Index of the tabs to select.
    const std::string selected_tabs;

    // Index to move the tabs to.
    const int target_index;

    // Expected state after the move (space separated list of indices).
    const std::string state_after_move;
  } test_data[] = {
    // 1 selected tab.
    { 2, 0, "0", 1, "1 0" },
    { 3, 0, "0", 2, "1 2 0" },
    { 3, 0, "2", 0, "2 0 1" },
    { 3, 0, "2", 1, "0 2 1" },
    { 3, 0, "0 1", 0, "0 1 2" },

    // 2 selected tabs.
    { 6, 0, "4 5", 1, "0 4 5 1 2 3" },
    { 3, 0, "0 1", 1, "2 0 1" },
    { 4, 0, "0 2", 1, "1 0 2 3" },
    { 6, 0, "0 1", 3, "2 3 4 0 1 5" },

    // 3 selected tabs.
    { 6, 0, "0 2 3", 3, "1 4 5 0 2 3" },
    { 7, 0, "4 5 6", 1, "0 4 5 6 1 2 3" },
    { 7, 0, "1 5 6", 4, "0 2 3 4 1 5 6" },

    // 5 selected tabs.
    { 8, 0, "0 2 3 6 7", 3, "1 4 5 0 2 3 6 7" },

    // 7 selected tabs
    { 16, 0, "0 1 2 3 4 7 9", 8, "5 6 8 10 11 12 13 14 0 1 2 3 4 7 9 15" },

    // With pinned tabs.
    { 6, 2, "2 3", 2, "0p 1p 2 3 4 5" },
    { 6, 2, "0 4", 3, "1p 0p 2 3 4 5" },
    { 6, 3, "1 2 4", 0, "1p 2p 0p 4 3 5" },
    { 8, 3, "1 3 4", 4, "0p 2p 1p 5 6 3 4 7" },

    { 7, 4, "2 3 4", 3, "0p 1p 2p 3p 5 4 6" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    TabStripDummyDelegate delegate(NULL);
    TabStripModel strip(&delegate, profile());
    ASSERT_NO_FATAL_FAILURE(
        PrepareTabstripForSelectionTest(&strip, test_data[i].tab_count,
                                        test_data[i].pinned_count,
                                        test_data[i].selected_tabs));
    strip.MoveSelectedTabsTo(test_data[i].target_index);
    EXPECT_EQ(test_data[i].state_after_move, GetPinnedState(strip)) << i;
    strip.CloseAllTabs();
  }
}

TEST_F(TabStripModelTest, CloseSelectedTabs) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());
  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();
  strip.AppendTabContents(contents1, true);
  strip.AppendTabContents(contents2, true);
  strip.AppendTabContents(contents3, true);
  strip.ToggleSelectionAt(1);
  strip.CloseSelectedTabs();
  EXPECT_EQ(1, strip.count());
  EXPECT_EQ(0, strip.active_index());
  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, MultipleSelection) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer(&strip);
  TabContentsWrapper* contents0 = CreateTabContents();
  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  TabContentsWrapper* contents3 = CreateTabContents();
  strip.AppendTabContents(contents0, false);
  strip.AppendTabContents(contents1, false);
  strip.AppendTabContents(contents2, false);
  strip.AppendTabContents(contents3, false);
  observer.set_log_tab_selection_changed(true);
  strip.AddObserver(&observer);

  // Selection and active tab change.
  strip.ActivateTabAt(3, true);
  ASSERT_EQ(2, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0)->action,
            MockTabStripModelObserver::ACTIVATE);
  MockTabStripModelObserver::State s1(contents3, 3,
      MockTabStripModelObserver::SELECT);
  EXPECT_TRUE(observer.StateEquals(1, s1));
  observer.ClearStates();

  // Adding all tabs to selection, active tab is now at 0.
  strip.ExtendSelectionTo(0);
  ASSERT_EQ(2, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0)->action,
            MockTabStripModelObserver::ACTIVATE);
  MockTabStripModelObserver::State s2(contents0, 0,
      MockTabStripModelObserver::SELECT);
  s2.src_contents = contents3;
  s2.src_index = 3;
  EXPECT_TRUE(observer.StateEquals(1, s2));
  observer.ClearStates();

  // Closing one of the selected tabs, not the active one.
  strip.CloseTabContentsAt(1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(3, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0)->action,
            MockTabStripModelObserver::CLOSE);
  ASSERT_EQ(observer.GetStateAt(1)->action,
            MockTabStripModelObserver::DETACH);
  ASSERT_EQ(observer.GetStateAt(2)->action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Closing the active tab, while there are others tabs selected.
  strip.CloseTabContentsAt(0, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(2, strip.count());
  ASSERT_EQ(4, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0)->action,
            MockTabStripModelObserver::CLOSE);
  ASSERT_EQ(observer.GetStateAt(1)->action,
            MockTabStripModelObserver::DETACH);
  ASSERT_EQ(observer.GetStateAt(2)->action,
            MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(3)->action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Active tab is at 0, deselecting all but the active tab.
  strip.ToggleSelectionAt(1);
  ASSERT_EQ(1, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0)->action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Attempting to deselect the only selected and therefore active tab,
  // it is ignored (no notifications being sent) and tab at 0 remains selected
  // and active.
  strip.ToggleSelectionAt(0);
  ASSERT_EQ(0, observer.GetStateCount());

  strip.RemoveObserver(&observer);
  strip.CloseAllTabs();
}

// Verifies that if we change the selection from a multi selection to a single
// selection, but not in a way that changes the selected_index that
// TabSelectionChanged is invoked.
TEST_F(TabStripModelTest, MultipleToSingle) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile());
  TabContentsWrapper* contents1 = CreateTabContents();
  TabContentsWrapper* contents2 = CreateTabContents();
  strip.AppendTabContents(contents1, false);
  strip.AppendTabContents(contents2, false);
  strip.ToggleSelectionAt(0);
  strip.ToggleSelectionAt(1);

  MockTabStripModelObserver observer(&strip);
  observer.set_log_tab_selection_changed(true);
  strip.AddObserver(&observer);
  // This changes the selection (0 is no longer selected) but the selected_index
  // still remains at 1.
  strip.ActivateTabAt(1, true);
  ASSERT_EQ(1, observer.GetStateCount());
  MockTabStripModelObserver::State s(
      contents2, 1, MockTabStripModelObserver::SELECT);
  s.src_contents = contents2;
  s.src_index = 1;
  s.user_gesture = false;
  EXPECT_TRUE(observer.StateEquals(0, s));
  strip.RemoveObserver(&observer);
  strip.CloseAllTabs();
}
