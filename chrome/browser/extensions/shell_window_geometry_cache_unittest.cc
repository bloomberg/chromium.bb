// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/mock_pref_change_callback.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/shell_window_geometry_cache.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
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
        ui_thread_(BrowserThread::UI, &ui_message_loop_) {
    prefs_.reset(new TestExtensionPrefs(ui_message_loop_.message_loop_proxy()));
    cache_.reset(
        new ShellWindowGeometryCache(&profile_, prefs_->prefs()));
    cache_->SetSyncDelayForTests(0);
  }

  void AddGeometryAndLoadExtension(const std::string& extension_id,
                                   const std::string& window_id,
                                   const gfx::Rect& bounds);

  // Spins the UI threads' message loops to make sure any task
  // posted to sync the geometry to the value store gets a chance to run.
  void WaitForSync();

  void LoadExtension(const std::string& extension_id);
  void UnloadExtension(const std::string& extension_id);

 protected:
  TestingProfile profile_;
  MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestExtensionPrefs> prefs_;
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
  prefs_->prefs()->SetGeometryCache(extension_id, dict.Pass());
  LoadExtension(extension_id);
}

void ShellWindowGeometryCacheTest::WaitForSync() {
  content::RunAllPendingInMessageLoop();
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
  const std::string extension_id = prefs_->AddExtensionAndReturnId("ext1");
  gfx::Rect bounds;
  ASSERT_FALSE(cache_->GetGeometry(extension_id, kWindowId, &bounds));
}

// Test getting geometry for an unknown extension.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryUnkownExtension) {
  const std::string extension_id1 = prefs_->AddExtensionAndReturnId("ext1");
  const std::string extension_id2 = prefs_->AddExtensionAndReturnId("ext2");
  AddGeometryAndLoadExtension(extension_id1, kWindowId,
                              gfx::Rect(4, 5, 31, 43));
  gfx::Rect bounds;
  ASSERT_FALSE(cache_->GetGeometry(extension_id2, kWindowId, &bounds));
}

// Test getting geometry for an unknown window in a known extension.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryUnkownWindow) {
  const std::string extension_id = prefs_->AddExtensionAndReturnId("ext1");
  AddGeometryAndLoadExtension(extension_id, kWindowId,
                              gfx::Rect(4, 5, 31, 43));
  gfx::Rect bounds;
  ASSERT_FALSE(cache_->GetGeometry(extension_id, kWindowId2, &bounds));
}

// Test that loading geometry from the store works correctly.
TEST_F(ShellWindowGeometryCacheTest, GetGeometryFromStore) {
  const std::string extension_id = prefs_->AddExtensionAndReturnId("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  AddGeometryAndLoadExtension(extension_id, kWindowId, bounds);
  gfx::Rect newBounds;
  ASSERT_TRUE(cache_->GetGeometry(extension_id, kWindowId, &newBounds));
  ASSERT_EQ(bounds, newBounds);
}

// Test saving geometry to the cache and state store, and reading it back.
TEST_F(ShellWindowGeometryCacheTest, SaveGeometryToStore) {
  const std::string extension_id = prefs_->AddExtensionAndReturnId("ext1");
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
  const base::DictionaryValue* dict =
      prefs_->prefs()->GetGeometryCache(extension_id);
  ASSERT_TRUE(dict);

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
  using testing::_;
  using testing::Mock;

  const std::string extension_id = prefs_->AddExtensionAndReturnId("ext1");
  gfx::Rect bounds1(100, 200, 300, 400);
  gfx::Rect bounds2(200, 400, 600, 800);
  gfx::Rect bounds2_duplicate(200, 400, 600, 800);

  MockPrefChangeCallback observer(prefs_->pref_service());
  PrefChangeRegistrar registrar;
  registrar.Init(prefs_->pref_service());
  registrar.Add("extensions.settings", observer.GetCallback());

  // Write the first bounds - it should do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id, kWindowId, bounds1);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different bounds - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id, kWindowId, bounds2);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a bounds that's a duplicate of what we already have. This should
  // not do any writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
  cache_->SaveGeometry(extension_id, kWindowId, bounds2_duplicate);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);
}

// Tests that no more than kMaxCachedWindows windows will be cached.
TEST_F(ShellWindowGeometryCacheTest, MaxWindows) {
  const std::string extension_id = prefs_->AddExtensionAndReturnId("ext1");
  // inform cache of extension
  LoadExtension(extension_id);

  gfx::Rect bounds(4, 5, 31, 43);
  for (size_t i = 0; i < ShellWindowGeometryCache::kMaxCachedWindows + 1; ++i) {
    std::string window_id = "window_" + base::IntToString(i);
    cache_->SaveGeometry(extension_id, window_id, bounds);
  }

  // The first added window should no longer have cached geometry.
  EXPECT_FALSE(cache_->GetGeometry(extension_id, "window_0", &bounds));
  // All other windows should still exist.
  for (size_t i = 1; i < ShellWindowGeometryCache::kMaxCachedWindows + 1; ++i) {
    std::string window_id = "window_" + base::IntToString(i);
    EXPECT_TRUE(cache_->GetGeometry(extension_id, window_id, &bounds));
  }
}

} // namespace extensions
