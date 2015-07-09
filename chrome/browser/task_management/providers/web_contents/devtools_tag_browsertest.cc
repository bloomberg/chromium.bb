// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/task_management/providers/task_provider_observer.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_task_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace task_management {

namespace {

const char kTestPage1[] = "files/devtools/debugger_test_page.html";
const char kTestPage2[] = "files/devtools/navigate_back.html";

}  // namespace

// Defines a browser test for testing that DevTools WebContents are being tagged
// properly by a DevToolsTag and that the TagsManager records these tags. It
// will also test that the WebContentsTaskProvider will be able to provide the
// appropriate DevToolsTask.
class DevToolsTagTest
    : public InProcessBrowserTest,
      public TaskProviderObserver {
 public:
  DevToolsTagTest()
      : devtools_window_(nullptr),
        provided_tasks_() {
    CHECK(test_server()->Start());
  }

  ~DevToolsTagTest() override {}

  void LoadTestPage(const std::string& test_page) {
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDevToolsWindow(bool is_docked) {
    devtools_window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(
        browser()->tab_strip_model()->GetWebContentsAt(0), is_docked);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window_);
  }

  // task_management::Task_providerObserver:
  void TaskAdded(Task* task) override {
    CHECK(task);
    ASSERT_FALSE(provided_tasks_.count(task));
    provided_tasks_.insert(task);
  }

  void TaskRemoved(Task* task) override {
    CHECK(task);
    ASSERT_TRUE(provided_tasks_.count(task));
    provided_tasks_.erase(task);
  }

  WebContentsTagsManager* tags_manager() const {
    return WebContentsTagsManager::GetInstance();
  }

  const std::set<Task*>& provided_tasks() const { return provided_tasks_; }

 private:
  DevToolsWindow* devtools_window_;
  std::set<Task*> provided_tasks_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTagTest);
};

// Tests that opening a DevToolsWindow will result in tagging its main
// WebContents and that tag will be recorded by the TagsManager.
IN_PROC_BROWSER_TEST_F(DevToolsTagTest, TagsManagerRecordsATag) {
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());

  // Loading a page will not result in tagging its WebContents.
  // TODO(afakhry): Once we start tagging the tab contents, this will change.
  // Fix it.
  LoadTestPage(kTestPage1);
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());

  // Test both docked and undocked devtools.
  OpenDevToolsWindow(true);
  EXPECT_FALSE(tags_manager()->tracked_tags().empty());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  CloseDevToolsWindow();
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());

  // For the undocked devtools there will be two tags one for the main contents
  // and one for the toolbox contents
  OpenDevToolsWindow(false);
  EXPECT_FALSE(tags_manager()->tracked_tags().empty());
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  CloseDevToolsWindow();
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());
}

IN_PROC_BROWSER_TEST_F(DevToolsTagTest, DevToolsTaskIsProvided) {
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());

  WebContentsTaskProvider provider;
  provider.SetObserver(this);

  // Still empty, no pre-existing tasks.
  EXPECT_TRUE(provided_tasks().empty());

  LoadTestPage(kTestPage1);
  // TODO(afakhry): This will change soon.
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());

  OpenDevToolsWindow(true);
  EXPECT_FALSE(tags_manager()->tracked_tags().empty());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_FALSE(provided_tasks().empty());
  EXPECT_EQ(1U, provided_tasks().size());

  const Task* task = *provided_tasks().begin();
  EXPECT_EQ(Task::RENDERER, task->GetType());

  // Navigating to a new page will not change the title of the devtools main
  // WebContents.
  const base::string16 title1 = task->title();
  LoadTestPage(kTestPage2);
  const base::string16 title2 = task->title();
  EXPECT_EQ(title1, title2);

  CloseDevToolsWindow();
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());
}

}  // namespace task_management
