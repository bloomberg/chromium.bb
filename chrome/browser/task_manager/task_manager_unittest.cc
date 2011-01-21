// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include <string>

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

#if defined(OS_MACOSX)
// From task_manager.cc:
// Activity Monitor shows %cpu with one decimal digit -- be
// consistent with that.
const char* kZeroCPUUsage = "0.0";
#else
const char* kZeroCPUUsage = "0";
#endif

class TestResource : public TaskManager::Resource {
 public:
  TestResource() : refresh_called_(false) {}

  virtual std::wstring GetTitle() const { return L"test title"; }
  virtual SkBitmap GetIcon() const { return SkBitmap(); }
  virtual base::ProcessHandle GetProcess() const {
    return base::GetCurrentProcessHandle();
  }
  virtual Type GetType() const { return RENDERER; }
  virtual bool SupportNetworkUsage() const { return false; }
  virtual void SetSupportNetworkUsage() { NOTREACHED(); }
  virtual void Refresh() { refresh_called_ = true; }
  bool refresh_called() const { return refresh_called_; }
  void set_refresh_called(bool refresh_called) {
    refresh_called_ = refresh_called;
  }

 private:
  bool refresh_called_;
};

}  // namespace

class TaskManagerTest : public testing::Test {
};

TEST_F(TaskManagerTest, Basic) {
  TaskManager task_manager;
  TaskManagerModel* model = task_manager.model_;
  EXPECT_EQ(0, model->ResourceCount());
}

TEST_F(TaskManagerTest, Resources) {
  TaskManager task_manager;
  TaskManagerModel* model = task_manager.model_;

  TestResource resource1, resource2;

  task_manager.AddResource(&resource1);
  ASSERT_EQ(1, model->ResourceCount());
  EXPECT_TRUE(model->IsResourceFirstInGroup(0));
  EXPECT_EQ(ASCIIToUTF16("test title"), model->GetResourceTitle(0));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT),
            model->GetResourceNetworkUsage(0));
  EXPECT_EQ(ASCIIToUTF16(kZeroCPUUsage), model->GetResourceCPUUsage(0));

  task_manager.AddResource(&resource2);  // Will be in the same group.
  ASSERT_EQ(2, model->ResourceCount());
  EXPECT_TRUE(model->IsResourceFirstInGroup(0));
  EXPECT_FALSE(model->IsResourceFirstInGroup(1));
  EXPECT_EQ(ASCIIToUTF16("test title"), model->GetResourceTitle(1));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
            model->GetResourceNetworkUsage(1));
  EXPECT_EQ(ASCIIToUTF16(kZeroCPUUsage), model->GetResourceCPUUsage(1));

  task_manager.RemoveResource(&resource1);
  // Now resource2 will be first in group.
  ASSERT_EQ(1, model->ResourceCount());
  EXPECT_TRUE(model->IsResourceFirstInGroup(0));
  EXPECT_EQ(ASCIIToUTF16("test title"), model->GetResourceTitle(0));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT),
            model->GetResourceNetworkUsage(0));
  EXPECT_EQ(ASCIIToUTF16(kZeroCPUUsage), model->GetResourceCPUUsage(0));

  task_manager.RemoveResource(&resource2);
  EXPECT_EQ(0, model->ResourceCount());
}

// Tests that the model is calling Refresh() on its resources.
TEST_F(TaskManagerTest, RefreshCalled) {
  MessageLoop loop;
  TaskManager task_manager;
  TaskManagerModel* model = task_manager.model_;
  TestResource resource;

  task_manager.AddResource(&resource);
  ASSERT_FALSE(resource.refresh_called());
  model->update_state_ = TaskManagerModel::TASK_PENDING;
  model->Refresh();
  ASSERT_TRUE(resource.refresh_called());
  task_manager.RemoveResource(&resource);
}
