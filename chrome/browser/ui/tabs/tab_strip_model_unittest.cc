// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using web_modal::NativeWebContentsModalDialog;

namespace {

// Class used to delete a WebContents and TabStripModel when another WebContents
// is destroyed.
class DeleteWebContentsOnDestroyedObserver
    : public content::WebContentsObserver {
 public:
  // When |source| is deleted both |tab_to_delete| and |tab_strip| are deleted.
  // |tab_to_delete| and |tab_strip| may be NULL.
  DeleteWebContentsOnDestroyedObserver(WebContents* source,
                                       WebContents* tab_to_delete,
                                       TabStripModel* tab_strip)
      : WebContentsObserver(source),
        tab_to_delete_(tab_to_delete),
        tab_strip_(tab_strip) {
  }

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    WebContents* tab_to_delete = tab_to_delete_;
    tab_to_delete_ = NULL;
    TabStripModel* tab_strip_to_delete = tab_strip_;
    tab_strip_ = NULL;
    delete tab_to_delete;
    delete tab_strip_to_delete;
  }

 private:
  WebContents* tab_to_delete_;
  TabStripModel* tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(DeleteWebContentsOnDestroyedObserver);
};

class TabStripDummyDelegate : public TestTabStripModelDelegate {
 public:
  TabStripDummyDelegate() : run_unload_(false) {}
  virtual ~TabStripDummyDelegate() {}

  void set_run_unload_listener(bool value) { run_unload_ = value; }

  virtual bool RunUnloadListenerBeforeClosing(WebContents* contents) OVERRIDE {
    return run_unload_;
  }

 private:
  // Whether to report that we need to run an unload listener before closing.
  bool run_unload_;

  DISALLOW_COPY_AND_ASSIGN(TabStripDummyDelegate);
};

const char kTabStripModelTestIDUserDataKey[] = "TabStripModelTestIDUserData";

class TabStripModelTestIDUserData : public base::SupportsUserData::Data {
 public:
  explicit TabStripModelTestIDUserData(int id) : id_(id) {}
  virtual ~TabStripModelTestIDUserData() {}
  int id() { return id_; }

 private:
  int id_;
};

class DummyNativeWebContentsModalDialogManager
    : public web_modal::NativeWebContentsModalDialogManager {
 public:
  explicit DummyNativeWebContentsModalDialogManager(
      web_modal::NativeWebContentsModalDialogManagerDelegate* delegate)
      : delegate_(delegate) {}
  virtual ~DummyNativeWebContentsModalDialogManager() {}

  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {}
  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {}
  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {}
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    delegate_->WillClose(dialog);
  }
  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {}
  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {}
  virtual void HostChanged(
      web_modal::WebContentsModalDialogHost* new_host) OVERRIDE {}

 private:
  web_modal::NativeWebContentsModalDialogManagerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DummyNativeWebContentsModalDialogManager);
};

// Test Browser-like class for TabStripModelTest.TabBlockedState.
class TabBlockedStateTestBrowser
    : public TabStripModelObserver,
      public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  explicit TabBlockedStateTestBrowser(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }

  virtual ~TabBlockedStateTestBrowser() {
    tab_strip_model_->RemoveObserver(this);
  }

 private:
  // TabStripModelObserver
  virtual void TabInsertedAt(WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE {
    web_modal::WebContentsModalDialogManager* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(contents);
    if (manager)
      manager->SetDelegate(this);
  }

  // WebContentsModalDialogManagerDelegate
  virtual void SetWebContentsBlocked(content::WebContents* contents,
                                     bool blocked) OVERRIDE {
    int index = tab_strip_model_->GetIndexOfWebContents(contents);
    ASSERT_GE(index, 0);
    tab_strip_model_->SetTabBlocked(index, blocked);
  }

  TabStripModel* tab_strip_model_;

  DISALLOW_COPY_AND_ASSIGN(TabBlockedStateTestBrowser);
};

}  // namespace

class TabStripModelTest : public ChromeRenderViewHostTestHarness {
 public:
  WebContents* CreateWebContents() {
    return WebContents::Create(WebContents::CreateParams(profile()));
  }

  WebContents* CreateWebContentsWithSharedRPH(WebContents* web_contents) {
    WebContents::CreateParams create_params(
        profile(), web_contents->GetRenderViewHost()->GetSiteInstance());
    WebContents* retval = WebContents::Create(create_params);
    EXPECT_EQ(retval->GetRenderProcessHost(),
              web_contents->GetRenderProcessHost());
    return retval;
  }

  // Sets the id of the specified contents.
  void SetID(WebContents* contents, int id) {
    contents->SetUserData(&kTabStripModelTestIDUserDataKey,
                          new TabStripModelTestIDUserData(id));
  }

  // Returns the id of the specified contents.
  int GetID(WebContents* contents) {
    TabStripModelTestIDUserData* user_data =
        static_cast<TabStripModelTestIDUserData*>(
            contents->GetUserData(&kTabStripModelTestIDUserDataKey));

    return user_data ? user_data->id() : -1;
  }

  // Returns the state of the given tab strip as a string. The state consists
  // of the ID of each web contents followed by a 'p' if pinned. For example,
  // if the model consists of two tabs with ids 2 and 1, with the first
  // tab pinned, this returns "2p 1".
  std::string GetTabStripStateString(const TabStripModel& model) {
    std::string actual;
    for (int i = 0; i < model.count(); ++i) {
      if (i > 0)
        actual += " ";

      actual += base::IntToString(GetID(model.GetWebContentsAt(i)));

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
      WebContents* contents = CreateWebContents();
      SetID(contents, i);
      model->AppendWebContents(contents, true);
    }
    for (int i = 0; i < pinned_count; ++i)
      model->SetTabPinned(i, true);

    ui::ListSelectionModel selection_model;
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
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  explicit MockTabStripModelObserver(TabStripModel* model)
      : empty_(true),
        deleted_(false),
        model_(model) {}
  virtual ~MockTabStripModelObserver() {}

  enum TabStripModelObserverAction {
    INSERT,
    CLOSE,
    DETACH,
    ACTIVATE,
    DEACTIVATE,
    SELECT,
    MOVE,
    CHANGE,
    PINNED,
    REPLACED
  };

  struct State {
    State(WebContents* a_dst_contents,
          int a_dst_index,
          TabStripModelObserverAction a_action)
        : src_contents(NULL),
          dst_contents(a_dst_contents),
          src_index(-1),
          dst_index(a_dst_index),
          change_reason(CHANGE_REASON_NONE),
          foreground(false),
          action(a_action) {
    }

    WebContents* src_contents;
    WebContents* dst_contents;
    int src_index;
    int dst_index;
    int change_reason;
    bool foreground;
    TabStripModelObserverAction action;
  };

  int GetStateCount() const {
    return static_cast<int>(states_.size());
  }

  State GetStateAt(int index) const {
    DCHECK(index >= 0 && index < GetStateCount());
    return states_[index];
  }

  bool StateEquals(int index, const State& state) {
    State s = GetStateAt(index);
    return (s.src_contents == state.src_contents &&
            s.dst_contents == state.dst_contents &&
            s.src_index == state.src_index &&
            s.dst_index == state.dst_index &&
            s.change_reason == state.change_reason &&
            s.foreground == state.foreground &&
            s.action == state.action);
  }

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE {
    empty_ = false;
    State s(contents, index, INSERT);
    s.foreground = foreground;
    states_.push_back(s);
  }
  virtual void ActiveTabChanged(WebContents* old_contents,
                                WebContents* new_contents,
                                int index,
                                int reason) OVERRIDE {
    State s(new_contents, index, ACTIVATE);
    s.src_contents = old_contents;
    s.change_reason = reason;
    states_.push_back(s);
  }
  virtual void TabSelectionChanged(
      TabStripModel* tab_strip_model,
      const ui::ListSelectionModel& old_model) OVERRIDE {
    State s(model()->GetActiveWebContents(), model()->active_index(), SELECT);
    s.src_contents = model()->GetWebContentsAt(old_model.active());
    s.src_index = old_model.active();
    states_.push_back(s);
  }
  virtual void TabMoved(WebContents* contents,
                        int from_index,
                        int to_index) OVERRIDE {
    State s(contents, to_index, MOVE);
    s.src_index = from_index;
    states_.push_back(s);
  }

  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            WebContents* contents,
                            int index) OVERRIDE {
    states_.push_back(State(contents, index, CLOSE));
  }
  virtual void TabDetachedAt(WebContents* contents, int index) OVERRIDE {
    states_.push_back(State(contents, index, DETACH));
  }
  virtual void TabDeactivated(WebContents* contents) OVERRIDE {
    states_.push_back(State(contents, model()->active_index(), DEACTIVATE));
  }
  virtual void TabChangedAt(WebContents* contents,
                            int index,
                            TabChangeType change_type) OVERRIDE {
    states_.push_back(State(contents, index, CHANGE));
  }
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             WebContents* old_contents,
                             WebContents* new_contents,
                             int index) OVERRIDE {
    State s(new_contents, index, REPLACED);
    s.src_contents = old_contents;
    states_.push_back(s);
  }
  virtual void TabPinnedStateChanged(WebContents* contents,
                                     int index) OVERRIDE {
    states_.push_back(State(contents, index, PINNED));
  }
  virtual void TabStripEmpty() OVERRIDE {
    empty_ = true;
  }
  virtual void TabStripModelDeleted() OVERRIDE {
    deleted_ = true;
  }

  void ClearStates() {
    states_.clear();
  }

  bool empty() const { return empty_; }
  bool deleted() const { return deleted_; }
  TabStripModel* model() { return model_; }

 private:
  std::vector<State> states_;

  bool empty_;
  bool deleted_;
  TabStripModel* model_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

TEST_F(TabStripModelTest, TestBasicAPI) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer(&tabstrip);
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  WebContents* contents1 = CreateWebContents();
  SetID(contents1, 1);

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Test AppendWebContents, ContainsIndex
  {
    EXPECT_FALSE(tabstrip.ContainsIndex(0));
    tabstrip.AppendWebContents(contents1, true);
    EXPECT_TRUE(tabstrip.ContainsIndex(0));
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(3, observer.GetStateCount());
    State s1(contents1, 0, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents1, 0, MockTabStripModelObserver::ACTIVATE);
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(contents1, 0, MockTabStripModelObserver::SELECT);
    s3.src_contents = NULL;
    s3.src_index = ui::ListSelectionModel::kUnselectedIndex;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    observer.ClearStates();
  }
  EXPECT_EQ("1", GetTabStripStateString(tabstrip));

  // Test InsertWebContentsAt, foreground tab.
  WebContents* contents2 = CreateWebContents();
  SetID(contents2, 2);
  {
    tabstrip.InsertWebContentsAt(1, contents2, TabStripModel::ADD_ACTIVE);

    EXPECT_EQ(2, tabstrip.count());
    EXPECT_EQ(4, observer.GetStateCount());
    State s1(contents2, 1, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents1, 0, MockTabStripModelObserver::DEACTIVATE);
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s3.src_contents = contents1;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    State s4(contents2, 1, MockTabStripModelObserver::SELECT);
    s4.src_contents = contents1;
    s4.src_index = 0;
    EXPECT_TRUE(observer.StateEquals(3, s4));
    observer.ClearStates();
  }
  EXPECT_EQ("1 2", GetTabStripStateString(tabstrip));

  // Test InsertWebContentsAt, background tab.
  WebContents* contents3 = CreateWebContents();
  SetID(contents3, 3);
  {
    tabstrip.InsertWebContentsAt(2, contents3, TabStripModel::ADD_NONE);

    EXPECT_EQ(3, tabstrip.count());
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::INSERT);
    s1.foreground = false;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

  // Test ActivateTabAt
  {
    tabstrip.ActivateTabAt(2, true);
    EXPECT_EQ(3, observer.GetStateCount());
    State s1(contents2, 1, MockTabStripModelObserver::DEACTIVATE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents3, 2, MockTabStripModelObserver::ACTIVATE);
    s2.src_contents = contents2;
    s2.change_reason = TabStripModelObserver::CHANGE_REASON_USER_GESTURE;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(contents3, 2, MockTabStripModelObserver::SELECT);
    s3.src_contents = contents2;
    s3.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    observer.ClearStates();
  }
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

  // Test DetachWebContentsAt
  {
    // Detach ...
    WebContents* detached = tabstrip.DetachWebContentsAt(2);
    // ... and append again because we want this for later.
    tabstrip.AppendWebContents(detached, true);
    EXPECT_EQ(8, observer.GetStateCount());
    State s1(detached, 2, MockTabStripModelObserver::DETACH);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(detached, ui::ListSelectionModel::kUnselectedIndex,
        MockTabStripModelObserver::DEACTIVATE);
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s3.src_contents = contents3;
    s3.change_reason = TabStripModelObserver::CHANGE_REASON_NONE;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    State s4(contents2, 1, MockTabStripModelObserver::SELECT);
    s4.src_contents = NULL;
    s4.src_index = ui::ListSelectionModel::kUnselectedIndex;
    EXPECT_TRUE(observer.StateEquals(3, s4));
    State s5(detached, 2, MockTabStripModelObserver::INSERT);
    s5.foreground = true;
    EXPECT_TRUE(observer.StateEquals(4, s5));
    State s6(contents2, 1, MockTabStripModelObserver::DEACTIVATE);
    EXPECT_TRUE(observer.StateEquals(5, s6));
    State s7(detached, 2, MockTabStripModelObserver::ACTIVATE);
    s7.src_contents = contents2;
    s7.change_reason = TabStripModelObserver::CHANGE_REASON_NONE;
    EXPECT_TRUE(observer.StateEquals(6, s7));
    State s8(detached, 2, MockTabStripModelObserver::SELECT);
    s8.src_contents = contents2;
    s8.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(7, s8));
    observer.ClearStates();
  }
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

  // Test CloseWebContentsAt
  {
    EXPECT_TRUE(tabstrip.CloseWebContentsAt(2, TabStripModel::CLOSE_NONE));
    EXPECT_EQ(2, tabstrip.count());

    EXPECT_EQ(5, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::CLOSE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents3, 2, MockTabStripModelObserver::DETACH);
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(contents3, ui::ListSelectionModel::kUnselectedIndex,
        MockTabStripModelObserver::DEACTIVATE);
    EXPECT_TRUE(observer.StateEquals(2, s3));
    State s4(contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s4.src_contents = contents3;
    s4.change_reason = TabStripModelObserver::CHANGE_REASON_NONE;
    EXPECT_TRUE(observer.StateEquals(3, s4));
    State s5(contents2, 1, MockTabStripModelObserver::SELECT);
    s5.src_contents = NULL;
    s5.src_index = ui::ListSelectionModel::kUnselectedIndex;
    EXPECT_TRUE(observer.StateEquals(4, s5));
    observer.ClearStates();
  }
  EXPECT_EQ("1 2", GetTabStripStateString(tabstrip));

  // Test MoveWebContentsAt, select_after_move == true
  {
    tabstrip.MoveWebContentsAt(1, 0, true);

    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents2, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    EXPECT_EQ(0, tabstrip.active_index());
    observer.ClearStates();
  }
  EXPECT_EQ("2 1", GetTabStripStateString(tabstrip));

  // Test MoveWebContentsAt, select_after_move == false
  {
    tabstrip.MoveWebContentsAt(1, 0, false);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents1, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    EXPECT_EQ(1, tabstrip.active_index());

    tabstrip.MoveWebContentsAt(0, 1, false);
    observer.ClearStates();
  }
  EXPECT_EQ("2 1", GetTabStripStateString(tabstrip));

  // Test Getters
  {
    EXPECT_EQ(contents2, tabstrip.GetActiveWebContents());
    EXPECT_EQ(contents2, tabstrip.GetWebContentsAt(0));
    EXPECT_EQ(contents1, tabstrip.GetWebContentsAt(1));
    EXPECT_EQ(0, tabstrip.GetIndexOfWebContents(contents2));
    EXPECT_EQ(1, tabstrip.GetIndexOfWebContents(contents1));
  }

  // Test UpdateWebContentsStateAt
  {
    tabstrip.UpdateWebContentsStateAt(0, TabStripModelObserver::ALL);
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
    // |CloseSelectedTabs| calls CloseWebContentsAt, we already tested that, now
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
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // This is a basic test of opener functionality. opener is created
  // as the first tab in the strip and then we create 5 other tabs in the
  // background with opener set as their opener.

  WebContents* opener = CreateWebContents();
  tabstrip.AppendWebContents(opener, true);
  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();
  WebContents* contents4 = CreateWebContents();
  WebContents* contents5 = CreateWebContents();

  // We use |InsertWebContentsAt| here instead of |AppendWebContents| so that
  // openership relationships are preserved.
  tabstrip.InsertWebContentsAt(tabstrip.count(), contents1,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertWebContentsAt(tabstrip.count(), contents2,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertWebContentsAt(tabstrip.count(), contents3,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertWebContentsAt(tabstrip.count(), contents4,
                               TabStripModel::ADD_INHERIT_GROUP);
  tabstrip.InsertWebContentsAt(tabstrip.count(), contents5,
                               TabStripModel::ADD_INHERIT_GROUP);

  // All the tabs should have the same opener.
  for (int i = 1; i < tabstrip.count(); ++i)
    EXPECT_EQ(opener, tabstrip.GetOpenerOfWebContentsAt(i));

  // If there is a next adjacent item, then the index should be of that item.
  EXPECT_EQ(2, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 1, false));
  // If the last tab in the group is closed, the preceding tab in the same
  // group should be selected.
  EXPECT_EQ(4, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 5, false));

  // Tests the method that finds the last tab opened by the same opener in the
  // strip (this is the insertion index for the next background tab for the
  // specified opener).
  EXPECT_EQ(5, tabstrip.GetIndexOfLastWebContentsOpenedBy(opener, 1));

  // For a tab that has opened no other tabs, the return value should always be
  // -1...
  EXPECT_EQ(-1,
            tabstrip.GetIndexOfNextWebContentsOpenedBy(contents1, 3, false));
  EXPECT_EQ(-1,
            tabstrip.GetIndexOfLastWebContentsOpenedBy(contents1, 3));

  // ForgetAllOpeners should destroy all opener relationships.
  tabstrip.ForgetAllOpeners();
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 1, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 5, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(opener, 1));

  // Specify the last tab as the opener of the others.
  for (int i = 0; i < tabstrip.count() - 1; ++i)
    tabstrip.SetOpenerOfWebContentsAt(i, contents5);

  for (int i = 0; i < tabstrip.count() - 1; ++i)
    EXPECT_EQ(contents5, tabstrip.GetOpenerOfWebContentsAt(i));

  // If there is a next adjacent item, then the index should be of that item.
  EXPECT_EQ(2, tabstrip.GetIndexOfNextWebContentsOpenedBy(contents5, 1, false));

  // If the last tab in the group is closed, the preceding tab in the same
  // group should be selected.
  EXPECT_EQ(3, tabstrip.GetIndexOfNextWebContentsOpenedBy(contents5, 4, false));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

static int GetInsertionIndex(TabStripModel* tabstrip) {
  return tabstrip->order_controller()->DetermineInsertionIndex(
      content::PAGE_TRANSITION_LINK, false);
}

static void InsertWebContentses(TabStripModel* tabstrip,
                                WebContents* contents1,
                                WebContents* contents2,
                                WebContents* contents3) {
  tabstrip->InsertWebContentsAt(GetInsertionIndex(tabstrip),
                                contents1,
                                TabStripModel::ADD_INHERIT_GROUP);
  tabstrip->InsertWebContentsAt(GetInsertionIndex(tabstrip),
                                contents2,
                                TabStripModel::ADD_INHERIT_GROUP);
  tabstrip->InsertWebContentsAt(GetInsertionIndex(tabstrip),
                                contents3,
                                TabStripModel::ADD_INHERIT_GROUP);
}

// Tests opening background tabs.
TEST_F(TabStripModelTest, TestLTRInsertionOptions) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  WebContents* opener = CreateWebContents();
  tabstrip.AppendWebContents(opener, true);

  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();

  // Test LTR
  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(contents1, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(contents2, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(contents3, tabstrip.GetWebContentsAt(3));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// This test constructs a tabstrip, and then simulates loading several tabs in
// the background from link clicks on the first tab. Then it simulates opening
// a new tab from the first tab in the foreground via a link click, verifies
// that this tab is opened adjacent to the opener, then closes it.
// Finally it tests that a tab opened for some non-link purpose opens at the
// end of the strip, not bundled to any existing context.
TEST_F(TabStripModelTest, TestInsertionIndexDetermination) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  WebContents* opener = CreateWebContents();
  tabstrip.AppendWebContents(opener, true);

  // Open some other random unrelated tab in the background to monkey with our
  // insertion index.
  WebContents* other = CreateWebContents();
  tabstrip.AppendWebContents(other, false);

  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();

  // Start by testing LTR.
  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(opener, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(contents1, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(contents2, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(contents3, tabstrip.GetWebContentsAt(3));
  EXPECT_EQ(other, tabstrip.GetWebContentsAt(4));

  // The opener API should work...
  EXPECT_EQ(3, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 2, false));
  EXPECT_EQ(2, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(3, tabstrip.GetIndexOfLastWebContentsOpenedBy(opener, 1));

  // Now open a foreground tab from a link. It should be opened adjacent to the
  // opener tab.
  WebContents* fg_link_contents = CreateWebContents();
  int insert_index = tabstrip.order_controller()->DetermineInsertionIndex(
      content::PAGE_TRANSITION_LINK, true);
  EXPECT_EQ(1, insert_index);
  tabstrip.InsertWebContentsAt(insert_index, fg_link_contents,
                               TabStripModel::ADD_ACTIVE |
                               TabStripModel::ADD_INHERIT_GROUP);
  EXPECT_EQ(1, tabstrip.active_index());
  EXPECT_EQ(fg_link_contents, tabstrip.GetActiveWebContents());

  // Now close this contents. The selection should move to the opener contents.
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(0, tabstrip.active_index());

  // Now open a new empty tab. It should open at the end of the strip.
  WebContents* fg_nonlink_contents = CreateWebContents();
  insert_index = tabstrip.order_controller()->DetermineInsertionIndex(
      content::PAGE_TRANSITION_AUTO_BOOKMARK, true);
  EXPECT_EQ(tabstrip.count(), insert_index);
  // We break the opener relationship...
  tabstrip.InsertWebContentsAt(insert_index,
                               fg_nonlink_contents,
                               TabStripModel::ADD_NONE);
  // Now select it, so that user_gesture == true causes the opener relationship
  // to be forgotten...
  tabstrip.ActivateTabAt(tabstrip.count() - 1, true);
  EXPECT_EQ(tabstrip.count() - 1, tabstrip.active_index());
  EXPECT_EQ(fg_nonlink_contents, tabstrip.GetActiveWebContents());

  // Verify that all opener relationships are forgotten.
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 2, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(opener, 1));

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
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  WebContents* opener = CreateWebContents();
  tabstrip.AppendWebContents(opener, true);

  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();

  // Note that we use Detach instead of Close throughout this test to avoid
  // having to keep reconstructing these WebContentses.

  // First test that closing tabs that are in the background doesn't adjust the
  // current selection.
  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.DetachWebContentsAt(1);
  EXPECT_EQ(0, tabstrip.active_index());

  for (int i = tabstrip.count() - 1; i >= 1; --i)
    tabstrip.DetachWebContentsAt(i);

  // Now test that when a tab doesn't have an opener, selection shifts to the
  // right when the tab is closed.
  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tabstrip.ActivateTabAt(1, true);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.DetachWebContentsAt(1);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.DetachWebContentsAt(1);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.DetachWebContentsAt(1);
  EXPECT_EQ(0, tabstrip.active_index());

  for (int i = tabstrip.count() - 1; i >= 1; --i)
    tabstrip.DetachWebContentsAt(i);

  // Now test that when a tab does have an opener, it selects the next tab
  // opened by the same opener scanning LTR when it is closed.
  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());
  tabstrip.ActivateTabAt(2, false);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());
  // Finally test that when a tab has no "siblings" that the opener is
  // selected.
  WebContents* other_contents = CreateWebContents();
  tabstrip.InsertWebContentsAt(1, other_contents,
                               TabStripModel::ADD_NONE);
  EXPECT_EQ(2, tabstrip.count());
  WebContents* opened_contents = CreateWebContents();
  tabstrip.InsertWebContentsAt(2, opened_contents,
                               TabStripModel::ADD_ACTIVE |
                               TabStripModel::ADD_INHERIT_GROUP);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTab.
TEST_F(TabStripModelTest, CommandCloseTab) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Make sure can_close is honored.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 1, 0, "0"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  ASSERT_TRUE(tabstrip.empty());

  // Make sure close on a tab that is selected affects all the selected tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  // Should have closed tabs 0 and 1.
  EXPECT_EQ("2", GetTabStripStateString(tabstrip));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select two tabs and make close on a tab that isn't selected doesn't affect
  // selected tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  2, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  // Should have closed tab 2.
  EXPECT_EQ("0 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Tests with 3 tabs, one pinned, two tab selected, one of which is pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  // Should have closed tab 2.
  EXPECT_EQ("2", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTabs.
TEST_F(TabStripModelTest, CommandCloseOtherTabs) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs, select two tabs, CommandCloseOtherTabs should be enabled
  // and close two tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ("0 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select two tabs, CommandCloseOtherTabs should be enabled and invoking it
  // with a non-selected index should close the two other tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
                  2, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ("0 1", GetTabStripStateString(tabstrip));
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
  EXPECT_EQ("0p 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTabsToRight.
TEST_F(TabStripModelTest, CommandCloseTabsToRight) {
  TabStripDummyDelegate delegate;
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
  EXPECT_EQ("0", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandTogglePinned.
TEST_F(TabStripModelTest, CommandTogglePinned) {
  TabStripDummyDelegate delegate;
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
  EXPECT_EQ("0p 1p 2", GetTabStripStateString(tabstrip));

  // Execute CommandTogglePinned again, this should unpin.
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("0 1 2", GetTabStripStateString(tabstrip));

  // Pin the last.
  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("2p 0 1", GetTabStripStateString(tabstrip));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests the following context menu commands:
//  - Close Tab
//  - Close Other Tabs
//  - Close Tabs To Right
TEST_F(TabStripModelTest, TestContextMenuCloseCommands) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  WebContents* opener = CreateWebContents();
  tabstrip.AppendWebContents(opener, true);

  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();

  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  EXPECT_EQ(3, tabstrip.count());

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsToRight);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(opener, tabstrip.GetActiveWebContents());

  WebContents* dummy = CreateWebContents();
  tabstrip.AppendWebContents(dummy, false);

  contents1 = CreateWebContents();
  contents2 = CreateWebContents();
  contents3 = CreateWebContents();
  InsertWebContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(5, tabstrip.count());

  int dummy_index = tabstrip.count() - 1;
  tabstrip.ActivateTabAt(dummy_index, true);
  EXPECT_EQ(dummy, tabstrip.GetActiveWebContents());

  tabstrip.ExecuteContextMenuCommand(dummy_index,
                                     TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(dummy, tabstrip.GetActiveWebContents());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests GetIndicesClosedByCommand.
TEST_F(TabStripModelTest, GetIndicesClosedByCommand) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();
  WebContents* contents4 = CreateWebContents();
  WebContents* contents5 = CreateWebContents();

  tabstrip.AppendWebContents(contents1, true);
  tabstrip.AppendWebContents(contents2, true);
  tabstrip.AppendWebContents(contents3, true);
  tabstrip.AppendWebContents(contents4, true);
  tabstrip.AppendWebContents(contents5, true);

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

// Tests whether or not WebContentses are inserted in the correct position
// using this "smart" function with a simulated middle click action on a series
// of links on the home page.
TEST_F(TabStripModelTest, AddWebContents_MiddleClickLinksAndClose) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  WebContents* homepage_contents = CreateWebContents();
  tabstrip.AddWebContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  WebContents* typed_page_contents = CreateWebContents();
  tabstrip.AddWebContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  WebContents* middle_click_contents1 = CreateWebContents();
  tabstrip.AddWebContents(
      middle_click_contents1, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  WebContents* middle_click_contents2 = CreateWebContents();
  tabstrip.AddWebContents(
      middle_click_contents2, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  WebContents* middle_click_contents3 = CreateWebContents();
  tabstrip.AddWebContents(
      middle_click_contents3, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);

  EXPECT_EQ(5, tabstrip.count());

  EXPECT_EQ(homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(middle_click_contents1, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(middle_click_contents2, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(middle_click_contents3, tabstrip.GetWebContentsAt(3));
  EXPECT_EQ(typed_page_contents, tabstrip.GetWebContentsAt(4));

  // Now simulate selecting a tab in the middle of the group of tabs opened from
  // the home page and start closing them. Each WebContents in the group
  // should be closed, right to left. This test is constructed to start at the
  // middle WebContents in the group to make sure the cursor wraps around
  // to the first WebContents in the group before closing the opener or
  // any other WebContents.
  tabstrip.ActivateTabAt(2, true);
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents3, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents1, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(homepage_contents, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(typed_page_contents, tabstrip.GetActiveWebContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not a WebContents created by a left click on a link
// that opens a new tab is inserted correctly adjacent to the tab that spawned
// it.
TEST_F(TabStripModelTest, AddWebContents_LeftClickPopup) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  WebContents* homepage_contents = CreateWebContents();
  tabstrip.AddWebContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  WebContents* typed_page_contents = CreateWebContents();
  tabstrip.AddWebContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a tab by simulating a left click on a link that opens in a new tab.
  WebContents* left_click_contents = CreateWebContents();
  tabstrip.AddWebContents(left_click_contents, -1,
                          content::PAGE_TRANSITION_LINK,
                          TabStripModel::ADD_ACTIVE);

  // Verify the state meets our expectations.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(left_click_contents, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(typed_page_contents, tabstrip.GetWebContentsAt(2));

  // The newly created tab should be selected.
  EXPECT_EQ(left_click_contents, tabstrip.GetActiveWebContents());

  // After closing the selected tab, the selection should move to the left, to
  // the opener.
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(homepage_contents, tabstrip.GetActiveWebContents());

  EXPECT_EQ(2, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not new tabs that should split context (typed pages,
// generated urls, also blank tabs) open at the end of the tabstrip instead of
// in the middle.
TEST_F(TabStripModelTest, AddWebContents_CreateNewBlankTab) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  WebContents* homepage_contents = CreateWebContents();
  tabstrip.AddWebContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  WebContents* typed_page_contents = CreateWebContents();
  tabstrip.AddWebContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a new blank tab in the foreground.
  WebContents* new_blank_contents = CreateWebContents();
  tabstrip.AddWebContents(new_blank_contents, -1,
                          content::PAGE_TRANSITION_TYPED,
                          TabStripModel::ADD_ACTIVE);

  // Verify the state of the tabstrip.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(typed_page_contents, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(new_blank_contents, tabstrip.GetWebContentsAt(2));

  // Now open a couple more blank tabs in the background.
  WebContents* background_blank_contents1 = CreateWebContents();
  tabstrip.AddWebContents(
      background_blank_contents1, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_NONE);
  WebContents* background_blank_contents2 = CreateWebContents();
  tabstrip.AddWebContents(
      background_blank_contents2, -1, content::PAGE_TRANSITION_GENERATED,
      TabStripModel::ADD_NONE);
  EXPECT_EQ(5, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(typed_page_contents, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(new_blank_contents, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(background_blank_contents1, tabstrip.GetWebContentsAt(3));
  EXPECT_EQ(background_blank_contents2, tabstrip.GetWebContentsAt(4));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether opener state is correctly forgotten when the user switches
// context.
TEST_F(TabStripModelTest, AddWebContents_ForgetOpeners) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  WebContents* homepage_contents = CreateWebContents();
  tabstrip.AddWebContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  WebContents* typed_page_contents = CreateWebContents();
  tabstrip.AddWebContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0, true);

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  WebContents* middle_click_contents1 = CreateWebContents();
  tabstrip.AddWebContents(
      middle_click_contents1, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  WebContents* middle_click_contents2 = CreateWebContents();
  tabstrip.AddWebContents(
      middle_click_contents2, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);
  WebContents* middle_click_contents3 = CreateWebContents();
  tabstrip.AddWebContents(
      middle_click_contents3, -1, content::PAGE_TRANSITION_LINK,
      TabStripModel::ADD_NONE);

  // Break out of the context by selecting a tab in a different context.
  EXPECT_EQ(typed_page_contents, tabstrip.GetWebContentsAt(4));
  tabstrip.SelectLastTab();
  EXPECT_EQ(typed_page_contents, tabstrip.GetActiveWebContents());

  // Step back into the context by selecting a tab inside it.
  tabstrip.ActivateTabAt(2, true);
  EXPECT_EQ(middle_click_contents2, tabstrip.GetActiveWebContents());

  // Now test that closing tabs selects to the right until there are no more,
  // then to the left, as if there were no context (context has been
  // successfully forgotten).
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents3, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(typed_page_contents, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(middle_click_contents1, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(homepage_contents, tabstrip.GetActiveWebContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Added for http://b/issue?id=958960
TEST_F(TabStripModelTest, AppendContentsReselectionTest) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  WebContents* homepage_contents = CreateWebContents();
  tabstrip.AddWebContents(
      homepage_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Open some other tab, by user typing.
  WebContents* typed_page_contents = CreateWebContents();
  tabstrip.AddWebContents(
      typed_page_contents, -1, content::PAGE_TRANSITION_TYPED,
      TabStripModel::ADD_NONE);

  // The selected tab should still be the first.
  EXPECT_EQ(0, tabstrip.active_index());

  // Now simulate a link click that opens a new tab (by virtue of target=_blank)
  // and make sure the correct tab gets selected when the new tab is closed.
  WebContents* target_blank = CreateWebContents();
  tabstrip.AppendWebContents(target_blank, true);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  // Clean up after ourselves.
  tabstrip.CloseAllTabs();
}

// Added for http://b/issue?id=1027661
TEST_F(TabStripModelTest, ReselectionConsidersChildrenTest) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A
  WebContents* page_a_contents = CreateWebContents();
  strip.AddWebContents(
      page_a_contents, -1, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      TabStripModel::ADD_ACTIVE);

  // Simulate middle click to open page A.A and A.B
  WebContents* page_a_a_contents = CreateWebContents();
  strip.AddWebContents(page_a_a_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  WebContents* page_a_b_contents = CreateWebContents();
  strip.AddWebContents(page_a_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Select page A.A
  strip.ActivateTabAt(1, true);
  EXPECT_EQ(page_a_a_contents, strip.GetActiveWebContents());

  // Simulate a middle click to open page A.A.A
  WebContents* page_a_a_a_contents = CreateWebContents();
  strip.AddWebContents(page_a_a_a_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  EXPECT_EQ(page_a_a_a_contents, strip.GetWebContentsAt(2));

  // Close page A.A
  strip.CloseWebContentsAt(strip.active_index(), TabStripModel::CLOSE_NONE);

  // Page A.A.A should be selected, NOT A.B
  EXPECT_EQ(page_a_a_a_contents, strip.GetActiveWebContents());

  // Close page A.A.A
  strip.CloseWebContentsAt(strip.active_index(), TabStripModel::CLOSE_NONE);

  // Page A.B should be selected
  EXPECT_EQ(page_a_b_contents, strip.GetActiveWebContents());

  // Close page A.B
  strip.CloseWebContentsAt(strip.active_index(), TabStripModel::CLOSE_NONE);

  // Page A should be selected
  EXPECT_EQ(page_a_contents, strip.GetActiveWebContents());

  // Clean up.
  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, AddWebContents_NewTabAtEndOfStripInheritsGroup) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A
  WebContents* page_a_contents = CreateWebContents();
  strip.AddWebContents(page_a_contents, -1,
                       content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       TabStripModel::ADD_ACTIVE);

  // Open pages B, C and D in the background from links on page A...
  WebContents* page_b_contents = CreateWebContents();
  WebContents* page_c_contents = CreateWebContents();
  WebContents* page_d_contents = CreateWebContents();
  strip.AddWebContents(page_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddWebContents(page_c_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddWebContents(page_d_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Switch to page B's tab.
  strip.ActivateTabAt(1, true);

  // Open a New Tab at the end of the strip (simulate Ctrl+T)
  WebContents* new_contents = CreateWebContents();
  strip.AddWebContents(new_contents, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfWebContents(new_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the New Tab that was just opened. We should be returned to page B's
  // Tab...
  strip.CloseWebContentsAt(4, TabStripModel::CLOSE_NONE);

  EXPECT_EQ(1, strip.active_index());

  // Open a non-New Tab tab at the end of the strip, with a TYPED transition.
  // This is like typing a URL in the address bar and pressing Alt+Enter. The
  // behavior should be the same as above.
  WebContents* page_e_contents = CreateWebContents();
  strip.AddWebContents(page_e_contents, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfWebContents(page_e_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the Tab. Selection should shift back to page B's Tab.
  strip.CloseWebContentsAt(4, TabStripModel::CLOSE_NONE);

  EXPECT_EQ(1, strip.active_index());

  // Open a non-New Tab tab at the end of the strip, with some other
  // transition. This is like right clicking on a bookmark and choosing "Open
  // in New Tab". No opener relationship should be preserved between this Tab
  // and the one that was active when the gesture was performed.
  WebContents* page_f_contents = CreateWebContents();
  strip.AddWebContents(page_f_contents, -1,
                       content::PAGE_TRANSITION_AUTO_BOOKMARK,
                       TabStripModel::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfWebContents(page_f_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the Tab. The next-adjacent should be selected.
  strip.CloseWebContentsAt(4, TabStripModel::CLOSE_NONE);

  EXPECT_EQ(3, strip.active_index());

  // Clean up.
  strip.CloseAllTabs();
}

// A test of navigations in a tab that is part of a group of opened from some
// parent tab. If the navigations are link clicks, the group relationship of
// the tab to its parent are preserved. If they are of any other type, they are
// not preserved.
TEST_F(TabStripModelTest, NavigationForgetsOpeners) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A
  WebContents* page_a_contents = CreateWebContents();
  strip.AddWebContents(page_a_contents, -1,
                       content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       TabStripModel::ADD_ACTIVE);

  // Open pages B, C and D in the background from links on page A...
  WebContents* page_b_contents = CreateWebContents();
  WebContents* page_c_contents = CreateWebContents();
  WebContents* page_d_contents = CreateWebContents();
  strip.AddWebContents(page_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddWebContents(page_c_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddWebContents(page_d_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Open page E in a different opener group from page A.
  WebContents* page_e_contents = CreateWebContents();
  strip.AddWebContents(page_e_contents, -1,
                       content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       TabStripModel::ADD_NONE);

  // Tell the TabStripModel that we are navigating page D via a link click.
  strip.ActivateTabAt(3, true);
  strip.TabNavigating(page_d_contents, content::PAGE_TRANSITION_LINK);

  // Close page D, page C should be selected. (part of same group).
  strip.CloseWebContentsAt(3, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(2, strip.active_index());

  // Tell the TabStripModel that we are navigating in page C via a bookmark.
  strip.TabNavigating(page_c_contents, content::PAGE_TRANSITION_AUTO_BOOKMARK);

  // Close page C, page E should be selected. (C is no longer part of the
  // A-B-C-D group, selection moves to the right).
  strip.CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(page_e_contents, strip.GetWebContentsAt(strip.active_index()));

  strip.CloseAllTabs();
}

// A test that the forgetting behavior tested in NavigationForgetsOpeners above
// doesn't cause the opener relationship for a New Tab opened at the end of the
// TabStrip to be reset (Test 1 below), unless another any other tab is
// selected (Test 2 below).
TEST_F(TabStripModelTest, NavigationForgettingDoesntAffectNewTab) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open a tab and several tabs from it, then select one of the tabs that was
  // opened.
  WebContents* page_a_contents = CreateWebContents();
  strip.AddWebContents(page_a_contents, -1,
                       content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       TabStripModel::ADD_ACTIVE);

  WebContents* page_b_contents = CreateWebContents();
  WebContents* page_c_contents = CreateWebContents();
  WebContents* page_d_contents = CreateWebContents();
  strip.AddWebContents(page_b_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddWebContents(page_c_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  strip.AddWebContents(page_d_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  strip.ActivateTabAt(2, true);

  // TEST 1: If the user is in a group of tabs and opens a new tab at the end
  // of the strip, closing that new tab will select the tab that they were
  // last on.

  // Now simulate opening a new tab at the end of the TabStrip.
  WebContents* new_contents1 = CreateWebContents();
  strip.AddWebContents(new_contents1, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  // At this point, if we close this tab the last selected one should be
  // re-selected.
  strip.CloseWebContentsAt(strip.count() - 1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(page_c_contents, strip.GetWebContentsAt(strip.active_index()));

  // TEST 2: If the user is in a group of tabs and opens a new tab at the end
  // of the strip, selecting any other tab in the strip will cause that new
  // tab's opener relationship to be forgotten.

  // Open a new tab again.
  WebContents* new_contents2 = CreateWebContents();
  strip.AddWebContents(new_contents2, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  // Now select the first tab.
  strip.ActivateTabAt(0, true);

  // Now select the last tab.
  strip.ActivateTabAt(strip.count() - 1, true);

  // Now close the last tab. The next adjacent should be selected.
  strip.CloseWebContentsAt(strip.count() - 1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(page_d_contents, strip.GetWebContentsAt(strip.active_index()));

  strip.CloseAllTabs();
}

// This fails on Linux when run with the rest of unit_tests (crbug.com/302156)
// and fails consistently on Mac.
#if defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_FastShutdown \
    DISABLED_FastShutdown
#else
#define MAYBE_FastShutdown \
    FastShutdown
#endif
// Tests that fast shutdown is attempted appropriately.
TEST_F(TabStripModelTest, MAYBE_FastShutdown) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer(&tabstrip);
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  // Make sure fast shutdown is attempted when tabs that share a RPH are shut
  // down.
  {
    WebContents* contents1 = CreateWebContents();
    WebContents* contents2 = CreateWebContentsWithSharedRPH(contents1);

    SetID(contents1, 1);
    SetID(contents2, 2);

    tabstrip.AppendWebContents(contents1, true);
    tabstrip.AppendWebContents(contents2, true);

    // Turn on the fake unload listener so the tabs don't actually get shut
    // down when we call CloseAllTabs()---we need to be able to check that
    // fast shutdown was attempted.
    delegate.set_run_unload_listener(true);
    tabstrip.CloseAllTabs();
    // On a mock RPH this checks whether we *attempted* fast shutdown.
    // A real RPH would reject our attempt since there is an unload handler.
    EXPECT_TRUE(contents1->GetRenderProcessHost()->FastShutdownStarted());
    EXPECT_EQ(2, tabstrip.count());

    delegate.set_run_unload_listener(false);
    tabstrip.CloseAllTabs();
    EXPECT_TRUE(tabstrip.empty());
  }

  // Make sure fast shutdown is not attempted when only some tabs that share a
  // RPH are shut down.
  {
    WebContents* contents1 = CreateWebContents();
    WebContents* contents2 = CreateWebContentsWithSharedRPH(contents1);

    SetID(contents1, 1);
    SetID(contents2, 2);

    tabstrip.AppendWebContents(contents1, true);
    tabstrip.AppendWebContents(contents2, true);

    tabstrip.CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
    EXPECT_FALSE(contents1->GetRenderProcessHost()->FastShutdownStarted());
    EXPECT_EQ(1, tabstrip.count());

    tabstrip.CloseAllTabs();
    EXPECT_TRUE(tabstrip.empty());
  }
}

// Tests various permutations of apps.
TEST_F(TabStripModelTest, Apps) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer(&tabstrip);
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

#if defined(OS_WIN)
  base::FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  base::FilePath path(FILE_PATH_LITERAL("/foo"));
#endif

  DictionaryValue manifest;
  manifest.SetString("name", "hi!");
  manifest.SetString("version", "1");
  manifest.SetString("app.launch.web_url", "http://www.google.com");
  std::string error;
  scoped_refptr<Extension> extension_app(
      Extension::Create(path, extensions::Manifest::INVALID_LOCATION,
                        manifest, Extension::NO_FLAGS, &error));
  WebContents* contents1 = CreateWebContents();
  extensions::TabHelper::CreateForWebContents(contents1);
  extensions::TabHelper::FromWebContents(contents1)
      ->SetExtensionApp(extension_app.get());
  WebContents* contents2 = CreateWebContents();
  extensions::TabHelper::CreateForWebContents(contents2);
  extensions::TabHelper::FromWebContents(contents2)
      ->SetExtensionApp(extension_app.get());
  WebContents* contents3 = CreateWebContents();

  SetID(contents1, 1);
  SetID(contents2, 2);
  SetID(contents3, 3);

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Initial state, tab3 only and selected.
  tabstrip.AppendWebContents(contents3, true);

  observer.ClearStates();

  // Attempt to insert tab1 (an app tab) at position 1. This isn't a legal
  // position and tab1 should end up at position 0.
  {
    tabstrip.InsertWebContentsAt(1, contents1, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 0, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1ap 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Insert tab 2 at position 1.
  {
    tabstrip.InsertWebContentsAt(1, contents2, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents2, 1, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1ap 2ap 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab 3 to position 0. This isn't legal and should be ignored.
  {
    tabstrip.MoveWebContentsAt(2, 0, false);

    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state didn't change.
    EXPECT_EQ("1ap 2ap 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab 0 to position 3. This isn't legal and should be ignored.
  {
    tabstrip.MoveWebContentsAt(0, 2, false);

    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state didn't change.
    EXPECT_EQ("1ap 2ap 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab 0 to position 1. This is a legal move.
  {
    tabstrip.MoveWebContentsAt(0, 1, false);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 1, MockTabStripModelObserver::MOVE);
    state.src_index = 0;
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state didn't change.
    EXPECT_EQ("2ap 1ap 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Remove tab3 and insert at position 0. It should be forced to position 2.
  {
    tabstrip.DetachWebContentsAt(2);
    observer.ClearStates();

    tabstrip.InsertWebContentsAt(0, contents3, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents3, 2, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state didn't change.
    EXPECT_EQ("2ap 1ap 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  tabstrip.CloseAllTabs();
}

// Tests various permutations of pinning tabs.
TEST_F(TabStripModelTest, Pinning) {
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer(&tabstrip);
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();

  SetID(contents1, 1);
  SetID(contents2, 2);
  SetID(contents3, 3);

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Initial state, three tabs, first selected.
  tabstrip.AppendWebContents(contents1, true);
  tabstrip.AppendWebContents(contents2, false);
  tabstrip.AppendWebContents(contents3, false);

  observer.ClearStates();

  // Pin the first tab, this shouldn't visually reorder anything.
  {
    tabstrip.SetTabPinned(0, true);

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents1, 0, MockTabStripModelObserver::PINNED);
    EXPECT_TRUE(observer.StateEquals(0, state));

    // And verify the state.
    EXPECT_EQ("1p 2 3", GetTabStripStateString(tabstrip));

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
    EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

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
    EXPECT_EQ("3p 1 2", GetTabStripStateString(tabstrip));

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
    EXPECT_EQ("3p 1p 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab "2" to the front, it should be ignored.
  {
    tabstrip.MoveWebContentsAt(2, 0, false);

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state.
    EXPECT_EQ("3p 1p 2", GetTabStripStateString(tabstrip));

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
    EXPECT_EQ("1p 3 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Unpin tab "3", nothing should happen.
  {
    tabstrip.SetTabPinned(1, false);

    ASSERT_EQ(0, observer.GetStateCount());

    EXPECT_EQ("1p 3 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Pin "3" and "1".
  {
    tabstrip.SetTabPinned(0, true);
    tabstrip.SetTabPinned(1, true);

    EXPECT_EQ("1p 3p 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  WebContents* contents4 = CreateWebContents();
  SetID(contents4, 4);

  // Insert "4" between "1" and "3". As "1" and "4" are pinned, "4" should end
  // up after them.
  {
    tabstrip.InsertWebContentsAt(1, contents4, TabStripModel::ADD_NONE);

    ASSERT_EQ(1, observer.GetStateCount());
    State state(contents4, 2, MockTabStripModelObserver::INSERT);
    EXPECT_TRUE(observer.StateEquals(0, state));

    EXPECT_EQ("1p 3p 4 2", GetTabStripStateString(tabstrip));
  }

  tabstrip.CloseAllTabs();
}

// Makes sure the TabStripModel calls the right observer methods during a
// replace.
TEST_F(TabStripModelTest, ReplaceSendsSelected) {
  typedef MockTabStripModelObserver::State State;

  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());

  WebContents* first_contents = CreateWebContents();
  strip.AddWebContents(first_contents, -1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_ACTIVE);

  MockTabStripModelObserver tabstrip_observer(&strip);
  strip.AddObserver(&tabstrip_observer);

  WebContents* new_contents = CreateWebContents();
  delete strip.ReplaceWebContentsAt(0, new_contents);

  ASSERT_EQ(2, tabstrip_observer.GetStateCount());

  // First event should be for replaced.
  State state(new_contents, 0, MockTabStripModelObserver::REPLACED);
  state.src_contents = first_contents;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state));

  // And the second for selected.
  state = State(new_contents, 0, MockTabStripModelObserver::ACTIVATE);
  state.src_contents = first_contents;
  state.change_reason = TabStripModelObserver::CHANGE_REASON_REPLACED;
  EXPECT_TRUE(tabstrip_observer.StateEquals(1, state));

  // Now add another tab and replace it, making sure we don't get a selected
  // event this time.
  WebContents* third_contents = CreateWebContents();
  strip.AddWebContents(third_contents, 1, content::PAGE_TRANSITION_TYPED,
                       TabStripModel::ADD_NONE);

  tabstrip_observer.ClearStates();

  // And replace it.
  new_contents = CreateWebContents();
  delete strip.ReplaceWebContentsAt(1, new_contents);

  ASSERT_EQ(1, tabstrip_observer.GetStateCount());

  state = State(new_contents, 1, MockTabStripModelObserver::REPLACED);
  state.src_contents = third_contents;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state));

  strip.CloseAllTabs();
}

// Ensures discarding tabs leaves TabStripModel in a good state.
TEST_F(TabStripModelTest, DiscardWebContentsAt) {
  typedef MockTabStripModelObserver::State State;

  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  // Fill it with some tabs.
  WebContents* contents1 = CreateWebContents();
  tabstrip.AppendWebContents(contents1, true);
  WebContents* contents2 = CreateWebContents();
  tabstrip.AppendWebContents(contents2, true);

  // Start watching for events after the appends to avoid observing state
  // transitions that aren't relevant to this test.
  MockTabStripModelObserver tabstrip_observer(&tabstrip);
  tabstrip.AddObserver(&tabstrip_observer);

  // Discard one of the tabs.
  WebContents* null_contents1 = tabstrip.DiscardWebContentsAt(0);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));
  ASSERT_EQ(null_contents1, tabstrip.GetWebContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetWebContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.GetStateCount());
  State state1(null_contents1, 0, MockTabStripModelObserver::REPLACED);
  state1.src_contents = contents1;
  EXPECT_TRUE(tabstrip_observer.StateEquals(0, state1));
  tabstrip_observer.ClearStates();

  // Discard the same tab again.
  WebContents* null_contents2 = tabstrip.DiscardWebContentsAt(0);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));
  ASSERT_EQ(null_contents2, tabstrip.GetWebContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetWebContentsAt(1));
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

  // Don't discard active tab.
  tabstrip.DiscardWebContentsAt(0);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_FALSE(tabstrip.IsTabDiscarded(0));
  EXPECT_FALSE(tabstrip.IsTabDiscarded(1));

  tabstrip.CloseAllTabs();
}

// Makes sure TabStripModel handles the case of deleting a tab while removing
// another tab.
TEST_F(TabStripModelTest, DeleteFromDestroy) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());
  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  strip.AppendWebContents(contents1, true);
  strip.AppendWebContents(contents2, true);
  // DeleteWebContentsOnDestroyedObserver deletes contents1 when contents2 sends
  // out notification that it is being destroyed.
  DeleteWebContentsOnDestroyedObserver observer(contents2, contents1, NULL);
  strip.CloseAllTabs();
}

// Makes sure TabStripModel handles the case of deleting another tab and the
// TabStrip while removing another tab.
TEST_F(TabStripModelTest, DeleteTabStripFromDestroy) {
  TabStripDummyDelegate delegate;
  TabStripModel* strip = new TabStripModel(&delegate, profile());
  MockTabStripModelObserver tab_strip_model_observer(strip);
  strip->AddObserver(&tab_strip_model_observer);
  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  strip->AppendWebContents(contents1, true);
  strip->AppendWebContents(contents2, true);
  // DeleteWebContentsOnDestroyedObserver deletes |contents1| and |strip| when
  // |contents2| sends out notification that it is being destroyed.
  DeleteWebContentsOnDestroyedObserver observer(contents2, contents1, strip);
  strip->CloseAllTabs();
  EXPECT_TRUE(tab_strip_model_observer.empty());
  EXPECT_TRUE(tab_strip_model_observer.deleted());
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
    TabStripDummyDelegate delegate;
    TabStripModel strip(&delegate, profile());
    ASSERT_NO_FATAL_FAILURE(
        PrepareTabstripForSelectionTest(&strip, test_data[i].tab_count,
                                        test_data[i].pinned_count,
                                        test_data[i].selected_tabs));
    strip.MoveSelectedTabsTo(test_data[i].target_index);
    EXPECT_EQ(test_data[i].state_after_move,
              GetTabStripStateString(strip)) << i;
    strip.CloseAllTabs();
  }
}

// Tests that moving a tab forgets all groups referencing it.
TEST_F(TabStripModelTest, MoveSelectedTabsTo_ForgetGroups) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A as a new tab and then A1 in the background from A.
  WebContents* page_a_contents = CreateWebContents();
  strip.AddWebContents(page_a_contents, -1,
                       content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       TabStripModel::ADD_ACTIVE);
  WebContents* page_a1_contents = CreateWebContents();
  strip.AddWebContents(page_a1_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  // Likewise, open pages B and B1.
  WebContents* page_b_contents = CreateWebContents();
  strip.AddWebContents(page_b_contents, -1,
                       content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       TabStripModel::ADD_ACTIVE);
  WebContents* page_b1_contents = CreateWebContents();
  strip.AddWebContents(page_b1_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);

  EXPECT_EQ(page_a_contents, strip.GetWebContentsAt(0));
  EXPECT_EQ(page_a1_contents, strip.GetWebContentsAt(1));
  EXPECT_EQ(page_b_contents, strip.GetWebContentsAt(2));
  EXPECT_EQ(page_b1_contents, strip.GetWebContentsAt(3));

  // Move page B to the start of the tab strip.
  strip.MoveSelectedTabsTo(0);

  // Open page B2 in the background from B. It should end up after B.
  WebContents* page_b2_contents = CreateWebContents();
  strip.AddWebContents(page_b2_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  EXPECT_EQ(page_b_contents, strip.GetWebContentsAt(0));
  EXPECT_EQ(page_b2_contents, strip.GetWebContentsAt(1));
  EXPECT_EQ(page_a_contents, strip.GetWebContentsAt(2));
  EXPECT_EQ(page_a1_contents, strip.GetWebContentsAt(3));
  EXPECT_EQ(page_b1_contents, strip.GetWebContentsAt(4));

  // Switch to A.
  strip.ActivateTabAt(2, true);
  EXPECT_EQ(page_a_contents, strip.GetActiveWebContents());

  // Open page A2 in the background from A. It should end up after A1.
  WebContents* page_a2_contents = CreateWebContents();
  strip.AddWebContents(page_a2_contents, -1, content::PAGE_TRANSITION_LINK,
                       TabStripModel::ADD_NONE);
  EXPECT_EQ(page_b_contents, strip.GetWebContentsAt(0));
  EXPECT_EQ(page_b2_contents, strip.GetWebContentsAt(1));
  EXPECT_EQ(page_a_contents, strip.GetWebContentsAt(2));
  EXPECT_EQ(page_a1_contents, strip.GetWebContentsAt(3));
  EXPECT_EQ(page_a2_contents, strip.GetWebContentsAt(4));
  EXPECT_EQ(page_b1_contents, strip.GetWebContentsAt(5));

  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, CloseSelectedTabs) {
  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());
  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();
  strip.AppendWebContents(contents1, true);
  strip.AppendWebContents(contents2, true);
  strip.AppendWebContents(contents3, true);
  strip.ToggleSelectionAt(1);
  strip.CloseSelectedTabs();
  EXPECT_EQ(1, strip.count());
  EXPECT_EQ(0, strip.active_index());
  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, MultipleSelection) {
  typedef MockTabStripModelObserver::State State;

  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer(&strip);
  WebContents* contents0 = CreateWebContents();
  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  WebContents* contents3 = CreateWebContents();
  strip.AppendWebContents(contents0, false);
  strip.AppendWebContents(contents1, false);
  strip.AppendWebContents(contents2, false);
  strip.AppendWebContents(contents3, false);
  strip.AddObserver(&observer);

  // Selection and active tab change.
  strip.ActivateTabAt(3, true);
  ASSERT_EQ(2, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::ACTIVATE);
  State s1(contents3, 3, MockTabStripModelObserver::SELECT);
  EXPECT_TRUE(observer.StateEquals(1, s1));
  observer.ClearStates();

  // Adding all tabs to selection, active tab is now at 0.
  strip.ExtendSelectionTo(0);
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action,
            MockTabStripModelObserver::ACTIVATE);
  State s2(contents0, 0, MockTabStripModelObserver::SELECT);
  s2.src_contents = contents3;
  s2.src_index = 3;
  EXPECT_TRUE(observer.StateEquals(2, s2));
  observer.ClearStates();

  // Toggle the active tab, should make the next index active.
  strip.ToggleSelectionAt(0);
  EXPECT_EQ(1, strip.active_index());
  EXPECT_EQ(3U, strip.selection_model().size());
  EXPECT_EQ(4, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action,
            MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(2).action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Toggle the first tab back to selected and active.
  strip.ToggleSelectionAt(0);
  EXPECT_EQ(0, strip.active_index());
  EXPECT_EQ(4U, strip.selection_model().size());
  EXPECT_EQ(4, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action,
            MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(2).action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Closing one of the selected tabs, not the active one.
  strip.CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(3, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::CLOSE);
  ASSERT_EQ(observer.GetStateAt(1).action,
            MockTabStripModelObserver::DETACH);
  ASSERT_EQ(observer.GetStateAt(2).action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Closing the active tab, while there are others tabs selected.
  strip.CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(2, strip.count());
  ASSERT_EQ(5, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::CLOSE);
  ASSERT_EQ(observer.GetStateAt(1).action,
            MockTabStripModelObserver::DETACH);
  ASSERT_EQ(observer.GetStateAt(2).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(3).action,
            MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(4).action,
            MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Active tab is at 0, deselecting all but the active tab.
  strip.ToggleSelectionAt(1);
  ASSERT_EQ(1, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
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
  typedef MockTabStripModelObserver::State State;

  TabStripDummyDelegate delegate;
  TabStripModel strip(&delegate, profile());
  WebContents* contents1 = CreateWebContents();
  WebContents* contents2 = CreateWebContents();
  strip.AppendWebContents(contents1, false);
  strip.AppendWebContents(contents2, false);
  strip.ToggleSelectionAt(0);
  strip.ToggleSelectionAt(1);

  MockTabStripModelObserver observer(&strip);
  strip.AddObserver(&observer);
  // This changes the selection (0 is no longer selected) but the selected_index
  // still remains at 1.
  strip.ActivateTabAt(1, true);
  ASSERT_EQ(1, observer.GetStateCount());
  State s(contents2, 1, MockTabStripModelObserver::SELECT);
  s.src_contents = contents2;
  s.src_index = 1;
  s.change_reason = TabStripModelObserver::CHANGE_REASON_NONE;
  EXPECT_TRUE(observer.StateEquals(0, s));
  strip.RemoveObserver(&observer);
  strip.CloseAllTabs();
}

// Verifies a newly inserted tab retains its previous blocked state.
// http://crbug.com/276334
TEST_F(TabStripModelTest, TabBlockedState) {
  // Start with a source tab strip.
  TabStripDummyDelegate dummy_tab_strip_delegate;
  TabStripModel strip_src(&dummy_tab_strip_delegate, profile());
  TabBlockedStateTestBrowser browser_src(&strip_src);

  // Add a tab.
  WebContents* contents1 = CreateWebContents();
  web_modal::WebContentsModalDialogManager::CreateForWebContents(contents1);
  strip_src.AppendWebContents(contents1, false);

  // Add another tab.
  WebContents* contents2 = CreateWebContents();
  web_modal::WebContentsModalDialogManager::CreateForWebContents(contents2);
  strip_src.AppendWebContents(contents2, false);

  // Create a destination tab strip.
  TabStripModel strip_dst(&dummy_tab_strip_delegate, profile());
  TabBlockedStateTestBrowser browser_dst(&strip_dst);

  // Setup a NativeWebContentsModalDialogManager for tab |contents2|.
  web_modal::WebContentsModalDialogManager* modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(contents2);
  web_modal::WebContentsModalDialogManager::TestApi test_api(
      modal_dialog_manager);
  test_api.ResetNativeManager(
      new DummyNativeWebContentsModalDialogManager(modal_dialog_manager));

  // Show a dialog that blocks tab |contents2|.
  // DummyNativeWebContentsModalDialogManager doesn't care about the
  // NativeWebContentsModalDialog value, so any dummy value works.
  modal_dialog_manager->ShowDialog(
      reinterpret_cast<NativeWebContentsModalDialog>(0));
  EXPECT_TRUE(strip_src.IsTabBlocked(1));

  // Detach the tab.
  WebContents* moved_contents = strip_src.DetachWebContentsAt(1);
  EXPECT_EQ(contents2, moved_contents);

  // Attach the tab to the destination tab strip.
  strip_dst.AppendWebContents(moved_contents, true);
  EXPECT_TRUE(strip_dst.IsTabBlocked(0));

  strip_dst.CloseAllTabs();
  strip_src.CloseAllTabs();
}
