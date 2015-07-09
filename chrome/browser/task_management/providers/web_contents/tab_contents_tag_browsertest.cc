// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_management/task_management_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

namespace task_management {

namespace {

// Defines a test page file path along with its expected task manager reported
// values.
struct TestPageData {
  const char* page_file;
  const char* title;
  Task::Type task_type;
  int expected_prefix_message;
};

// The below test files are available in src/chrome/test/data/
// TODO(afakhry): Add more test pages here as needed (e.g. pages that are hosted
// in the tabs as apps or extensions).
const TestPageData kTestPages[] = {
    {
        "/title1.html",
        "",
        Task::RENDERER,
        IDS_TASK_MANAGER_TAB_PREFIX
    },
    {
        "/title2.html",
        "Title Of Awesomeness",
        Task::RENDERER,
        IDS_TASK_MANAGER_TAB_PREFIX
    },
    {
        "/title3.html",
        "Title Of More Awesomeness",
        Task::RENDERER,
        IDS_TASK_MANAGER_TAB_PREFIX
    },
};

const size_t kTestPagesLength = arraysize(kTestPages);

}  // namespace

// Defines a browser test class for testing the task manager tracking of tab
// contents.
class TabContentsTagTest : public InProcessBrowserTest {
 public:
  TabContentsTagTest() {
    EXPECT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }
  ~TabContentsTagTest() override {}

  void AddNewTestTabAt(int index, const char* test_page_file) {
    int tabs_count_before = tabs_count();
    GURL url = GetUrlOfFile(test_page_file);
    AddTabAtIndex(index, url, ui::PAGE_TRANSITION_TYPED);
    EXPECT_EQ(++tabs_count_before, tabs_count());
  }

  void NavigateToUrl(const char* test_page_file) {
    ui_test_utils::NavigateToURL(browser(), GetUrlOfFile(test_page_file));
  }

  void CloseTabAt(int index) {
    browser()->tab_strip_model()->CloseWebContentsAt(index,
                                                     TabStripModel::CLOSE_NONE);
  }

  base::string16 GetTestPageExpectedTitle(const TestPageData& page_data) const {
    // Pages with no title should fall back to their URL.
    base::string16 title = base::UTF8ToUTF16(page_data.title);
    if (title.empty()) {
      GURL url = GetUrlOfFile(page_data.page_file);
      title = base::UTF8ToUTF16(url.spec());
    }
    return l10n_util::GetStringFUTF16(page_data.expected_prefix_message, title);
  }

  base::string16 GetAboutBlankExpectedTitle() const {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                      base::UTF8ToUTF16("about:blank"));
  }

  int tabs_count() const { return browser()->tab_strip_model()->count(); }

  const std::vector<WebContentsTag*>& tracked_tags() const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }

 private:
  GURL GetUrlOfFile(const char* test_page_file) const {
    return embedded_test_server()->GetURL(test_page_file);
  }

  DISALLOW_COPY_AND_ASSIGN(TabContentsTagTest);
};

// Tests that TabContentsTags are being recorded correctly by the
// WebContentsTagsManager.
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, BasicTagsTracking) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());

  // Add a bunch of tabs and make sure we're tracking them.
  AddNewTestTabAt(0, kTestPages[0].page_file);
  EXPECT_EQ(2, tabs_count());
  EXPECT_EQ(2U, tracked_tags().size());

  AddNewTestTabAt(1, kTestPages[1].page_file);
  EXPECT_EQ(3, tabs_count());
  EXPECT_EQ(3U, tracked_tags().size());

  // Navigating the selected tab doesn't change the number of tabs nor the
  // number of tags.
  NavigateToUrl(kTestPages[2].page_file);
  EXPECT_EQ(3, tabs_count());
  EXPECT_EQ(3U, tracked_tags().size());

  // Close a bunch of tabs and make sure we can notice that.
  CloseTabAt(0);
  CloseTabAt(0);
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());
}

// Tests that the pre-task-manager-existing tabs are given to the task manager
// once it starts observing.
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, PreExistingTaskProviding) {
  // We start with the "about:blank" tab.
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());

  // Add a bunch of tabs and make sure when the task manager is created and
  // starts observing sees those pre-existing tabs.
  AddNewTestTabAt(0, kTestPages[0].page_file);
  EXPECT_EQ(2, tabs_count());
  EXPECT_EQ(2U, tracked_tags().size());
  AddNewTestTabAt(1, kTestPages[1].page_file);
  EXPECT_EQ(3, tabs_count());
  EXPECT_EQ(3U, tracked_tags().size());

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();
  EXPECT_EQ(3U, task_manager.tasks().size());
}

// Tests that the task manager sees the correct tabs with their correct
// corresponding tasks data.
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, PostExistingTaskProviding) {
  // We start with the "about:blank" tab.
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();
  EXPECT_EQ(1U, task_manager.tasks().size());

  const Task* first_tab_task = task_manager.tasks()[0];
  EXPECT_EQ(Task::RENDERER, first_tab_task->GetType());
  EXPECT_EQ(GetAboutBlankExpectedTitle(), first_tab_task->title());

  // Add the test pages in order and test the provided tasks.
  for (auto& test_page_data : kTestPages) {
    AddNewTestTabAt(0, test_page_data.page_file);

    const Task* task = task_manager.tasks().back();
    EXPECT_EQ(test_page_data.task_type, task->GetType());
    EXPECT_EQ(GetTestPageExpectedTitle(test_page_data), task->title());
  }

  EXPECT_EQ(1 + kTestPagesLength, task_manager.tasks().size());

  // Close the last tab that was added. Make sure it doesn't show up in the
  // task manager.
  CloseTabAt(0);
  EXPECT_EQ(kTestPagesLength, task_manager.tasks().size());
  const base::string16 closed_tab_title =
      GetTestPageExpectedTitle(kTestPages[kTestPagesLength - 1]);
  for (auto& task : task_manager.tasks())
    EXPECT_NE(closed_tab_title, task->title());
}

}  // namespace task_management
