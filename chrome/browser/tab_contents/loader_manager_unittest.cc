// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/loader_manager.h"
#include "chrome/browser/tab_contents/match_preview_loader.h"
#include "chrome/browser/tab_contents/match_preview_loader_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MatchPreviewLoaderDelegateImpl : public MatchPreviewLoaderDelegate {
 public:
  MatchPreviewLoaderDelegateImpl() {}

  virtual void ShowMatchPreviewLoader(MatchPreviewLoader* loader) {}

  virtual void SetSuggestedTextFor(MatchPreviewLoader* loader,
                                   const string16& text) {}

  virtual gfx::Rect GetMatchPreviewBounds() {
    return gfx::Rect();
  }

  virtual bool ShouldCommitPreviewOnMouseUp() {
    return false;
  }

  virtual void CommitPreview(MatchPreviewLoader* loader) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MatchPreviewLoaderDelegateImpl);
};

}

class LoaderManagerTest : public testing::Test {
 public:
  LoaderManagerTest() {}

  void MarkReady(MatchPreviewLoader* loader) {
    loader->ready_ = true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoaderManagerTest);
};

// Makes sure UpdateLoader works when invoked once.
TEST_F(LoaderManagerTest, Basic) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(0, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_TRUE(manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(0, manager.current_loader()->template_url_id());
}

// Make sure invoking update twice for non-instant results keeps the same
// loader.
TEST_F(LoaderManagerTest, UpdateTwice) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(0, &loader);
  MatchPreviewLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(0, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_EQ(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
}

// Make sure invoking update twice for instant results keeps the same loader.
TEST_F(LoaderManagerTest, UpdateInstantTwice) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(1, &loader);
  MatchPreviewLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(1, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_EQ(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Makes sure transitioning from non-instant to instant works.
TEST_F(LoaderManagerTest, NonInstantToInstant) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(0, &loader);
  MatchPreviewLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(1, &loader);
  EXPECT_TRUE(loader.get() != NULL);
  EXPECT_NE(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Makes sure instant loaders aren't deleted when invoking update with different
// ids.
TEST_F(LoaderManagerTest, DontDeleteInstantLoaders) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(1, &loader);
  MatchPreviewLoader* current_loader = manager.current_loader();
  manager.UpdateLoader(2, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_NE(current_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(2u, manager.num_instant_loaders());
}

// Makes sure a new loader is created and assigned to secondary when
// transitioning from a ready non-instant to instant.
TEST_F(LoaderManagerTest, CreateSecondaryWhenReady) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(0, &loader);
  MatchPreviewLoader* current_loader = manager.current_loader();
  ASSERT_TRUE(current_loader);
  MarkReady(current_loader);

  manager.UpdateLoader(1, &loader);
  EXPECT_EQ(NULL, loader.get());
  EXPECT_EQ(current_loader, manager.current_loader());
  EXPECT_TRUE(manager.pending_loader());
  EXPECT_NE(current_loader, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());

  // Make the pending loader current.
  MatchPreviewLoader* pending_loader = manager.pending_loader();
  manager.MakePendingCurrent(&loader);
  EXPECT_TRUE(loader.get());
  EXPECT_EQ(pending_loader, manager.current_loader());
  EXPECT_EQ(NULL, manager.pending_loader());
  EXPECT_EQ(1u, manager.num_instant_loaders());
}

// Makes sure releasing an instant updates maps currectly.
TEST_F(LoaderManagerTest, ReleaseInstant) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(1, &loader);
  scoped_ptr<MatchPreviewLoader> current_loader(manager.ReleaseCurrentLoader());
  EXPECT_TRUE(current_loader.get());
  EXPECT_EQ(NULL, manager.current_loader());
  EXPECT_EQ(0u, manager.num_instant_loaders());
}

// Tests transitioning from a non-instant ready loader to an instant ready
// loader is immediate.
TEST_F(LoaderManagerTest, NonInstantToInstantWhenReady) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(1, &loader);
  ASSERT_TRUE(manager.current_loader());
  EXPECT_EQ(1, manager.current_loader()->template_url_id());
  MatchPreviewLoader* instant_loader = manager.current_loader();

  manager.UpdateLoader(0, &loader);
  MatchPreviewLoader* non_instant_loader = manager.current_loader();
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
TEST_F(LoaderManagerTest, ThreeInstant) {
  MatchPreviewLoaderDelegateImpl delegate;
  LoaderManager manager(&delegate);
  scoped_ptr<MatchPreviewLoader> loader;
  manager.UpdateLoader(1, &loader);
  ASSERT_TRUE(manager.current_loader());
  EXPECT_EQ(1, manager.current_loader()->template_url_id());
  MatchPreviewLoader* instant_loader1 = manager.current_loader();
  MarkReady(instant_loader1);

  manager.UpdateLoader(2, &loader);
  MatchPreviewLoader* instant_loader2 = manager.pending_loader();
  ASSERT_TRUE(instant_loader2);
  EXPECT_EQ(2, instant_loader2->template_url_id());
  EXPECT_NE(instant_loader1, instant_loader2);
  EXPECT_EQ(instant_loader1, manager.current_loader());

  manager.UpdateLoader(3, &loader);
  MatchPreviewLoader* instant_loader3 = manager.pending_loader();
  ASSERT_TRUE(instant_loader3);
  EXPECT_EQ(3, instant_loader3->template_url_id());
  EXPECT_NE(instant_loader1, instant_loader3);
  EXPECT_NE(instant_loader2, instant_loader3);
  EXPECT_EQ(instant_loader1, manager.current_loader());
}
