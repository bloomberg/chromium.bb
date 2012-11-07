// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/shell_window_geometry_cache.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/value_store/testing_value_store.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  static const char kExtensionId[] = "extensionid";
  static const char kExtensionId2[] = "extensionid2";
  static const char kWindowId[] = "windowid";
  static const char kWindowId2[] = "windowid2";

  const char kWindowGeometryKey[] = "window_geometry";
} // namespace

using content::BrowserThread;

namespace extensions {

// Base class for tests.
class ShellWindowGeometryCacheTest : public testing::Test {
 public:
  ShellWindowGeometryCacheTest() :
        ui_thread_(BrowserThread::UI, &ui_message_loop_),
        file_thread_(BrowserThread::FILE),
        value_store_(new TestingValueStore()) {
    file_thread_.Start();
    state_store_.reset(new StateStore(&profile_, value_store_));
    cache_.reset(
        new ShellWindowGeometryCache(&profile_, state_store_.get()));
    cache_->SetSyncDelayForTests(0);
  }

  void AddGeometryAndLoadExtension(const std::string& extension_id,
                                   const std::string& window_id,
                                   const gfx::Rect& bounds);

  // Spins the UI and FILE threads' message loops to make sure any task
  // posted to sync the geometry to the value store gets a chance to run.
  void WaitForSync();

  void LoadExtension(const std::string& extension_id);
  void UnloadExtension(const std::string& extension_id);

 protected:
  TestingProfile profile_;
  MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  TestingValueStore* value_store_;
  scoped_ptr<StateStore> state_store_;
  scoped_ptr<ShellWindowGeometryCache> cache_;
};

void ShellWindowGeometryCacheTest::AddGeometryAndLoadExtension(
    const std::string& extension_id, const std::string& window_id,
    const gfx::Rect& bounds) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  base::DictionaryValue* value = new base::DictionaryValue;
  value->SetInteger("x", bounds.x());
  value->SetInteger("y", bounds.y());
  value->SetInteger("w", bounds.width());
  value->SetInteger("h", bounds.height());
  dict->SetWithoutPathExpansion(window_id, value);
  state_store_->SetExtensionValue(extension_id, kWindowGeometryKey,
                                  scoped_ptr<base::Value>(dict.release()));
  LoadExtension(extension_id);
}

void ShellWindowGeometryCacheTest::WaitForSync() {
  content::RunAllPendingInMessageLoop();
  content::RunAllPendingInMessageLoop(BrowserThread::FILE);
}

void ShellWindowGeometryCacheTest::LoadExtension(
    const std::string& extension_id) {
  cache_->OnExtensionLoaded(extension_id);
  WaitForSync();
}

void ShellWindowGeometryCacheTest::UnloadExtension(
    const std::string& extension_id) {
  cache_->OnExtensionUnloaded(extension_id);
  WaitForSync();
}

// Test getting geometry from an empty store.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryEmptyStore) {
  gfx::Rect bounds;
  ASSERT_FALSE(cache_->GetGeometry(kExtensionId, kWindowId, &bounds));
}

// Test getting geometry for an unknown extension.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryUnkownExtension) {
  AddGeometryAndLoadExtension(kExtensionId, kWindowId,
                              gfx::Rect(4, 5, 31, 43));
  gfx::Rect bounds;
  ASSERT_FALSE(cache_->GetGeometry(kExtensionId2, kWindowId, &bounds));
}

// Test getting geometry for an unknown window in a known extension.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryUnkownWindow) {
  AddGeometryAndLoadExtension(kExtensionId, kWindowId,
                              gfx::Rect(4, 5, 31, 43));
  gfx::Rect bounds;
  ASSERT_FALSE(cache_->GetGeometry(kExtensionId, kWindowId2, &bounds));
}

// Test that loading geometry from the store works correctly.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryFromStore) {
  gfx::Rect bounds(4, 5, 31, 43);
  AddGeometryAndLoadExtension(kExtensionId, kWindowId, bounds);
  gfx::Rect newBounds;
  ASSERT_TRUE(cache_->GetGeometry(kExtensionId, kWindowId, &newBounds));
  ASSERT_EQ(bounds, newBounds);
}

// Test saving geometry to the cache and state store, and reading it back.
TEST_F(ShellWindowGeometryCacheTest, SaveGeometryToStore) {
  const std::string extension_id(kExtensionId);
  const std::string window_id(kWindowId);

  // inform cache of extension
  LoadExtension(extension_id);

  // update geometry stored in cache
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect newBounds;
  cache_->SaveGeometry(extension_id, window_id, bounds);

  // make sure that immediately reading back geometry works
  ASSERT_TRUE(cache_->GetGeometry(extension_id, window_id, &newBounds));
  ASSERT_EQ(bounds, newBounds);

  // unload extension to force cache to save data to the state store
  UnloadExtension(extension_id);

  // check if geometry got stored correctly in the state store
  ValueStore::ReadResult result = value_store_->Get(extension_id + "." +
                                                    kWindowGeometryKey);
  ASSERT_FALSE(result->HasError());
  DictionaryValue* dict;
  ASSERT_TRUE(result->settings()->GetDictionaryWithoutPathExpansion(
      extension_id + "." + kWindowGeometryKey, &dict));

  ASSERT_TRUE(dict->HasKey(window_id));
  int v;
  ASSERT_TRUE(dict->GetInteger(window_id + ".x", &v));
  ASSERT_EQ(bounds.x(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".y", &v));
  ASSERT_EQ(bounds.y(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".w", &v));
  ASSERT_EQ(bounds.width(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".h", &v));
  ASSERT_EQ(bounds.height(), v);

  // check to make sure cache indeed doesn't know about this extension anymore
  ASSERT_FALSE(cache_->GetGeometry(extension_id, window_id, &newBounds));

  // reload extension
  LoadExtension(extension_id);
  // and make sure the geometry got reloaded properly too
  ASSERT_TRUE(cache_->GetGeometry(extension_id, window_id, &newBounds));
  ASSERT_EQ(bounds, newBounds);
}

// Tests that we won't do writes to the state store for SaveGeometry calls
// which don't change the state we already have.
TEST_F(ShellWindowGeometryCacheTest, NoDuplicateWrites) {
  gfx::Rect bounds1(100, 200, 300, 400);
  gfx::Rect bounds2(200, 400, 600, 800);
  gfx::Rect bounds2_duplicate(200, 400, 600, 800);

  // Write the first bounds - it should do > 0 writes.
  cache_->SaveGeometry(kExtensionId, kWindowId, bounds1);
  WaitForSync();
  int write_count1 = value_store_->write_count();
  EXPECT_GT(write_count1, 0);

  // Write a different bounds - it should also do > 0 writes.
  cache_->SaveGeometry(kExtensionId, kWindowId, bounds2);
  WaitForSync();
  int write_count2 = value_store_->write_count();
  EXPECT_GT(write_count2, write_count1);

  // Write a bounds that's a duplicate of what we already have. This should
  // not do any writes.
  cache_->SaveGeometry(kExtensionId, kWindowId, bounds2_duplicate);
  WaitForSync();
  int write_count3 = value_store_->write_count();
  EXPECT_EQ(write_count2, write_count3);
}

} // namespace extensions
