// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_bitmap.h"
#include "content/browser/android/system_ui_resource_manager_impl.h"
#include "content/public/browser/android/ui_resource_client_android.h"
#include "content/public/browser/android/ui_resource_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace content {

class TestSystemUIResourceManagerImpl
    : public content::SystemUIResourceManagerImpl {
 public:
  TestSystemUIResourceManagerImpl(content::UIResourceProvider* provider)
      : SystemUIResourceManagerImpl(provider) {}

  virtual ~TestSystemUIResourceManagerImpl() {}

  virtual void BuildResource(
      ui::SystemUIResourceManager::ResourceType type) OVERRIDE {}

  void SetResourceAsLoaded(ui::SystemUIResourceManager::ResourceType type) {
    SkBitmap small_bitmap;
    SkCanvas canvas(small_bitmap);
    small_bitmap.allocPixels(
        SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
    canvas.drawColor(SK_ColorWHITE);
    small_bitmap.setImmutable();
    OnFinishedLoadBitmap(type, &small_bitmap);
  }
};

namespace {

const ui::SystemUIResourceManager::ResourceType TEST_RESOURCE_TYPE =
    ui::SystemUIResourceManager::OVERSCROLL_GLOW;

class MockUIResourceProvider : public content::UIResourceProvider {
 public:
  MockUIResourceProvider()
      : next_ui_resource_id_(1),
        has_layer_tree_host_(true),
        system_ui_resource_manager_(this) {}

  virtual ~MockUIResourceProvider() {}

  virtual cc::UIResourceId CreateUIResource(
      content::UIResourceClientAndroid* client) OVERRIDE {
    if (!has_layer_tree_host_)
      return 0;
    cc::UIResourceId id = next_ui_resource_id_++;
    client->GetBitmap(id, false);
    ui_resource_client_map_[id] = client;
    return id;
  }

  virtual void DeleteUIResource(cc::UIResourceId id) OVERRIDE {
    CHECK(has_layer_tree_host_);
    ui_resource_client_map_.erase(id);
  }

  virtual bool SupportsETC1NonPowerOfTwo() const OVERRIDE { return true; }

  void LayerTreeHostCleared() {
    has_layer_tree_host_ = false;
    UIResourceClientMap client_map = ui_resource_client_map_;
    ui_resource_client_map_.clear();
    for (UIResourceClientMap::iterator iter = client_map.begin();
         iter != client_map.end();
         iter++) {
      iter->second->UIResourceIsInvalid();
    }
  }

  void LayerTreeHostReturned() { has_layer_tree_host_ = true; }

  TestSystemUIResourceManagerImpl& GetSystemUIResourceManager() {
    return system_ui_resource_manager_;
  }

  cc::UIResourceId next_ui_resource_id() const { return next_ui_resource_id_; }

 private:
  typedef base::hash_map<cc::UIResourceId, content::UIResourceClientAndroid*>
      UIResourceClientMap;

  cc::UIResourceId next_ui_resource_id_;
  UIResourceClientMap ui_resource_client_map_;
  bool has_layer_tree_host_;

  // The UIResourceProvider owns the SystemUIResourceManager.
  TestSystemUIResourceManagerImpl system_ui_resource_manager_;
};

}  // namespace

class SystemUIResourceManagerImplTest : public testing::Test {
 public:
  void PreloadResource(ui::SystemUIResourceManager::ResourceType type) {
    ui_resource_provider_.GetSystemUIResourceManager().PreloadResource(type);
  }

  cc::UIResourceId GetUIResourceId(
      ui::SystemUIResourceManager::ResourceType type) {
    return ui_resource_provider_.GetSystemUIResourceManager().GetUIResourceId(
        type);
  }

  void LayerTreeHostCleared() { ui_resource_provider_.LayerTreeHostCleared(); }

  void LayerTreeHostReturned() {
    ui_resource_provider_.LayerTreeHostReturned();
  }

  void SetResourceAsLoaded(ui::SystemUIResourceManager::ResourceType type) {
    ui_resource_provider_.GetSystemUIResourceManager().SetResourceAsLoaded(
        type);
  }

  cc::UIResourceId GetNextUIResourceId() const {
    return ui_resource_provider_.next_ui_resource_id();
  }

 private:
  MockUIResourceProvider ui_resource_provider_;
};

TEST_F(SystemUIResourceManagerImplTest, GetResourceAfterBitmapLoaded) {
  SetResourceAsLoaded(TEST_RESOURCE_TYPE);
  EXPECT_NE(0, GetUIResourceId(TEST_RESOURCE_TYPE));
}

TEST_F(SystemUIResourceManagerImplTest, GetResourceBeforeLoadBitmap) {
  EXPECT_EQ(0, GetUIResourceId(TEST_RESOURCE_TYPE));
  SetResourceAsLoaded(TEST_RESOURCE_TYPE);
  EXPECT_NE(0, GetUIResourceId(TEST_RESOURCE_TYPE));
}

TEST_F(SystemUIResourceManagerImplTest, PreloadEnsureResource) {
  // Preloading the resource should trigger bitmap loading, but the actual
  // resource id will not be generated until it is explicitly requested.
  cc::UIResourceId first_resource_id = GetNextUIResourceId();
  PreloadResource(TEST_RESOURCE_TYPE);
  SetResourceAsLoaded(TEST_RESOURCE_TYPE);
  EXPECT_EQ(first_resource_id, GetNextUIResourceId());
  EXPECT_NE(0, GetUIResourceId(TEST_RESOURCE_TYPE));
  EXPECT_NE(first_resource_id, GetNextUIResourceId());
}

TEST_F(SystemUIResourceManagerImplTest, ResetLayerTreeHost) {
  EXPECT_EQ(0, GetUIResourceId(TEST_RESOURCE_TYPE));
  LayerTreeHostCleared();
  EXPECT_EQ(0, GetUIResourceId(TEST_RESOURCE_TYPE));
  LayerTreeHostReturned();
  EXPECT_EQ(0, GetUIResourceId(TEST_RESOURCE_TYPE));

  SetResourceAsLoaded(TEST_RESOURCE_TYPE);
  EXPECT_NE(0, GetUIResourceId(TEST_RESOURCE_TYPE));
  LayerTreeHostCleared();
  EXPECT_EQ(0, GetUIResourceId(TEST_RESOURCE_TYPE));
  LayerTreeHostReturned();
  EXPECT_NE(0, GetUIResourceId(TEST_RESOURCE_TYPE));
}

}  // namespace content
