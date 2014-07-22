// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_background_task_queue.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// A ProcessManager that doesn't create background host pages.
class TestProcessManager : public ProcessManager {
 public:
  explicit TestProcessManager(Profile* profile)
      : ProcessManager(profile,
                       profile->GetOriginalProfile(),
                       ExtensionRegistry::Get(profile)),
        create_count_(0) {}
  virtual ~TestProcessManager() {}

  int create_count() { return create_count_; }

  // ProcessManager overrides:
  virtual bool CreateBackgroundHost(const Extension* extension,
                                    const GURL& url) OVERRIDE {
    // Don't actually try to create a web contents.
    create_count_++;
    return false;
  }

 private:
  int create_count_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

// Derives from ExtensionServiceTestBase because ExtensionService is difficult
// to initialize alone.
class LazyBackgroundTaskQueueTest
    : public extensions::ExtensionServiceTestBase {
 public:
  LazyBackgroundTaskQueueTest() : task_run_count_(0) {}
  virtual ~LazyBackgroundTaskQueueTest() {}

  int task_run_count() { return task_run_count_; }

  // A simple callback for AddPendingTask.
  void RunPendingTask(ExtensionHost* host) {
    task_run_count_++;
  }

  // Creates and registers an extension without a background page.
  scoped_refptr<Extension> CreateSimpleExtension() {
    scoped_refptr<Extension> extension = ExtensionBuilder()
        .SetManifest(DictionaryBuilder()
                     .Set("name", "No background")
                     .Set("version", "1")
                     .Set("manifest_version", 2))
        .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
        .Build();
    service_->AddExtension(extension);
    return extension;
  }

  // Creates and registers an extension with a lazy background page.
  scoped_refptr<Extension> CreateLazyBackgroundExtension() {
    scoped_refptr<Extension> extension = ExtensionBuilder()
        .SetManifest(DictionaryBuilder()
            .Set("name", "Lazy background")
            .Set("version", "1")
            .Set("manifest_version", 2)
            .Set("background",
                  DictionaryBuilder()
                  .Set("page", "background.html")
                  .SetBoolean("persistent", false)))
        .SetID("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
        .Build();
    service_->AddExtension(extension);
    return extension;
  }

 private:
  // The total number of pending tasks that have been executed.
  int task_run_count_;

  DISALLOW_COPY_AND_ASSIGN(LazyBackgroundTaskQueueTest);
};

// Tests that only extensions with background pages should have tasks queued.
TEST_F(LazyBackgroundTaskQueueTest, ShouldEnqueueTask) {
  InitializeEmptyExtensionService();
  InitializeProcessManager();

  LazyBackgroundTaskQueue queue(profile_.get());

  // Build a simple extension with no background page.
  scoped_refptr<Extension> no_background = CreateSimpleExtension();
  EXPECT_FALSE(queue.ShouldEnqueueTask(profile_.get(), no_background.get()));

  // Build another extension with a background page.
  scoped_refptr<Extension> with_background = CreateLazyBackgroundExtension();
  EXPECT_TRUE(queue.ShouldEnqueueTask(profile_.get(), with_background.get()));
}

// Tests that adding tasks actually increases the pending task count, and that
// multiple extensions can have pending tasks.
TEST_F(LazyBackgroundTaskQueueTest, AddPendingTask) {
  InitializeEmptyExtensionService();

  // Swap in our stub TestProcessManager.
  TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          ExtensionSystem::Get(profile_.get()));
  // Owned by |extension_system|.
  TestProcessManager* process_manager = new TestProcessManager(profile_.get());
  extension_system->SetProcessManager(process_manager);

  LazyBackgroundTaskQueue queue(profile_.get());

  // Build a simple extension with no background page.
  scoped_refptr<Extension> no_background = CreateSimpleExtension();

  // Adding a pending task increases the number of extensions with tasks, but
  // doesn't run the task.
  queue.AddPendingTask(profile_.get(),
                       no_background->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());
  EXPECT_EQ(0, task_run_count());

  // Another task on the same extension doesn't increase the number of
  // extensions that have tasks and doesn't run any tasks.
  queue.AddPendingTask(profile_.get(),
                       no_background->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());
  EXPECT_EQ(0, task_run_count());

  // Adding a task on an extension with a lazy background page tries to create
  // a background host, and if that fails, runs the task immediately.
  scoped_refptr<Extension> lazy_background = CreateLazyBackgroundExtension();
  queue.AddPendingTask(profile_.get(),
                       lazy_background->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(2u, queue.extensions_with_pending_tasks());
  // The process manager tried to create a background host.
  EXPECT_EQ(1, process_manager->create_count());
  // The task ran immediately because the creation failed.
  EXPECT_EQ(1, task_run_count());
}

// Tests that pending tasks are actually run.
TEST_F(LazyBackgroundTaskQueueTest, ProcessPendingTasks) {
  InitializeEmptyExtensionService();

  LazyBackgroundTaskQueue queue(profile_.get());

  // ProcessPendingTasks is a no-op if there are no tasks.
  scoped_refptr<Extension> extension = CreateSimpleExtension();
  queue.ProcessPendingTasks(NULL, profile_.get(), extension);
  EXPECT_EQ(0, task_run_count());

  // Schedule a task to run.
  queue.AddPendingTask(profile_.get(),
                       extension->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(0, task_run_count());
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());

  // Trying to run tasks for an unrelated profile should do nothing.
  TestingProfile profile2;
  queue.ProcessPendingTasks(NULL, &profile2, extension);
  EXPECT_EQ(0, task_run_count());
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());

  // Processing tasks when there is one pending runs the task and removes the
  // extension from the list of extensions with pending tasks.
  queue.ProcessPendingTasks(NULL, profile_.get(), extension);
  EXPECT_EQ(1, task_run_count());
  EXPECT_EQ(0u, queue.extensions_with_pending_tasks());
}

}  // namespace extensions
