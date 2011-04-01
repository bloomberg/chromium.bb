// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/instant/instant_loader_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class InstantLoaderDelegateImpl : public InstantLoaderDelegate {
 public:
  InstantLoaderDelegateImpl() {}

  virtual void InstantStatusChanged(InstantLoader* loader) OVERRIDE {}

  virtual void SetSuggestedTextFor(InstantLoader* loader,
                                   const string16& text,
                                   InstantCompleteBehavior behavior) OVERRIDE {}

  virtual gfx::Rect GetInstantBounds() OVERRIDE {
    return gfx::Rect();
  }

  virtual bool ShouldCommitInstantOnMouseUp() OVERRIDE {
    return false;
  }

  virtual void CommitInstantLoader(InstantLoader* loader) OVERRIDE {
  }

  virtual void InstantLoaderDoesntSupportInstant(
      InstantLoader* loader) OVERRIDE {
  }

  virtual void AddToBlacklist(InstantLoader* loader,
                              const GURL& url) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantLoaderDelegateImpl);
};

}

class InstantLoaderManagerTest : public testing::Test {
 public:
  InstantLoaderManagerTest() {}

  void MarkReady(InstantLoader* loader) {
    loader->ready_ = true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantLoaderManagerTest);
};

// Makes sure UpdateLoader works when invoked once.
TEST_F(InstantLoaderManagerTest, Basic) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(0, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_TRUE(manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(0, manager.current_loader()->template_url_id());
}

// Make sure invoking update twice for non-instant results keeps the same
// loader.
TEST_F(InstantLoaderManagerTest, UpdateTwice) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(0, &loader);
  InstantLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(0, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_EQ(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
}

// Make sure invoking update twice for instant results keeps the same loader.
TEST_F(InstantLoaderManagerTest, UpdateInstantTwice) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  InstantLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(1, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_EQ(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Makes sure transitioning from non-instant to instant works.
TEST_F(InstantLoaderManagerTest, NonInstantToInstant) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(0, &loader);
  InstantLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(1, &loader);
  EXPECT_TRUE(loader.get() != NULL);
  EXPECT_NE(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Makes sure instant loaders aren't deleted when invoking update with different
// ids.
TEST_F(InstantLoaderManagerTest, DontDeleteInstantLoaders) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  InstantLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(2, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_NE(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(2u, manager.num_instant_loaders());
}

// Makes sure a new loader is created and assigned to secondary when
// transitioning from a ready non-instant to instant.
TEST_F(InstantLoaderManagerTest, CreateSecondaryWhenReady) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(0, &loader);
  InstantLoader* current_loader = manager.current_loader();
  ASSERT_TRUE(current_loader);
  MarkReady(current_loader);

  manager.UpdateLoader(1, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_EQ(current_loader, manager.current_loader());
  EXPECT_TRUE(manager.pending_loader());
  EXPECT_NE(current_loader, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());

  // Make the pending loader current.
  InstantLoader* pending_loader = manager.pending_loader();
  manager.MakePendingCurrent(&loader);
  EXPECT_TRUE(loader.get());
  EXPECT_EQ(pending_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Makes sure releasing an instant updates maps currectly.
TEST_F(InstantLoaderManagerTest, ReleaseInstant) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  scoped_ptr<InstantLoader> current_loader(manager.ReleaseCurrentLoader());
  EXPECT_TRUE(current_loader.get());
  EXPECT_EQ(NULL, manager.current_loader());
  EXPECT_EQ(0u, manager.num_instant_loaders());
}

// Tests transitioning from a non-instant ready loader to an instant ready
// loader is immediate.
TEST_F(InstantLoaderManagerTest, NonInstantToInstantWhenReady) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  ASSERT_TRUE(manager.current_loader());
  EXPECT_EQ(1, manager.current_loader()->template_url_id());
  InstantLoader* instant_loader = manager.current_loader();

  manager.UpdateLoader(0, &loader);
  InstantLoader* non_instant_loader = manager.current_loader();
  ASSERT_TRUE(non_instant_loader);
  MarkReady(non_instant_loader);
  EXPECT_NE(non_instant_loader, instant_loader);

  MarkReady(instant_loader);
  manager.UpdateLoader(1, &loader);
  EXPECT_EQ(non_instant_loader, loader.get());
  EXPECT_EQ(instant_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Tests transitioning between 3 instant loaders, all ready.
TEST_F(InstantLoaderManagerTest, ThreeInstant) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  ASSERT_TRUE(manager.current_loader());
  EXPECT_EQ(1, manager.current_loader()->template_url_id());
  InstantLoader* instant_loader1 = manager.current_loader();
  MarkReady(instant_loader1);

  manager.UpdateLoader(2, &loader);
  InstantLoader* instant_loader2 = manager.pending_loader();
  ASSERT_TRUE(instant_loader2);
  EXPECT_EQ(2, instant_loader2->template_url_id());
  EXPECT_NE(instant_loader1, instant_loader2);
  EXPECT_EQ(instant_loader1, manager.current_loader());

  manager.UpdateLoader(3, &loader);
  InstantLoader* instant_loader3 = manager.pending_loader();
  ASSERT_TRUE(instant_loader3);
  EXPECT_EQ(3, instant_loader3->template_url_id());
  EXPECT_NE(instant_loader1, instant_loader3);
  EXPECT_NE(instant_loader2, instant_loader3);
  EXPECT_EQ(instant_loader1, manager.current_loader());
}

// Tests DestroyLoader with an instant loader.
TEST_F(InstantLoaderManagerTest, DestroyInstantLoader) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  ASSERT_TRUE(manager.current_loader());
  EXPECT_EQ(1, manager.current_loader()->template_url_id());
  // Now destroy it.
  manager.DestroyLoader(manager.current_loader());

  // There should be no current, pending and 0 instant loaders.
  ASSERT_EQ(NULL, manager.current_loader());
  ASSERT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(0u, manager.num_instant_loaders());
}

// Tests DestroyLoader when the loader is pending.
TEST_F(InstantLoaderManagerTest, DestroyPendingLoader) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(1, &loader);
  InstantLoader* first_loader = manager.active_loader();
  MarkReady(first_loader);

  // Create another loader.
  manager.UpdateLoader(0, &loader);
  InstantLoader* second_loader = manager.pending_loader();
  ASSERT_TRUE(second_loader);
  ASSERT_NE(second_loader, first_loader);

  // Destroy it.
  manager.DestroyLoader(second_loader);
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(first_loader, manager.current_loader());
}

// Makes sure WillUpateChangeActiveLoader works.
TEST_F(InstantLoaderManagerTest, WillUpateChangeActiveLoader) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);
  scoped_ptr<InstantLoader> loader;

  // When there is no loader WillUpateChangeActiveLoader should return true.
  EXPECT_TRUE(manager.WillUpateChangeActiveLoader(0));
  EXPECT_TRUE(manager.WillUpateChangeActiveLoader(1));

  // Add a loder with id 0 and test again.
  manager.UpdateLoader(0, &loader);
  EXPECT_FALSE(manager.WillUpateChangeActiveLoader(0));
  EXPECT_TRUE(manager.WillUpateChangeActiveLoader(1));
  ASSERT_TRUE(manager.active_loader());
  MarkReady(manager.active_loader());

  // Add a loader with id 1 and test again.
  manager.UpdateLoader(1, &loader);
  EXPECT_TRUE(manager.WillUpateChangeActiveLoader(0));
  EXPECT_FALSE(manager.WillUpateChangeActiveLoader(1));
}

// Makes sure UpdateLoader doesn't schedule a loader for deletion when asked
// to update and the pending loader is ready.
TEST_F(InstantLoaderManagerTest, UpdateWithReadyPending) {
  InstantLoaderDelegateImpl delegate;
  InstantLoaderManager manager(&delegate);

  {
    scoped_ptr<InstantLoader> loader;
    manager.UpdateLoader(1, &loader);
  }
  InstantLoader* instant_loader = manager.current_loader();
  ASSERT_TRUE(instant_loader);
  MarkReady(instant_loader);

  {
    scoped_ptr<InstantLoader> loader;
    manager.UpdateLoader(0, &loader);
  }
  InstantLoader* non_instant_loader = manager.active_loader();
  ASSERT_TRUE(non_instant_loader);
  ASSERT_NE(instant_loader, non_instant_loader);
  MarkReady(non_instant_loader);

  // This makes the non_instant_loader the current loader since it was ready.
  scoped_ptr<InstantLoader> loader;
  manager.UpdateLoader(0, &loader);
  ASSERT_NE(loader.get(), non_instant_loader);
}
