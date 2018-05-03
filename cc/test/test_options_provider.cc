// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_options_provider.h"

namespace cc {
class TestOptionsProvider::DiscardableManager
    : public SkStrikeServer::DiscardableHandleManager,
      public SkStrikeClient::DiscardableHandleManager {
 public:
  DiscardableManager() = default;
  ~DiscardableManager() override = default;

  // SkStrikeServer::DiscardableHandleManager implementation.
  SkDiscardableHandleId createHandle() override { return next_handle_id_++; }
  bool lockHandle(SkDiscardableHandleId handle_id) override {
    CHECK_LT(handle_id, next_handle_id_);
    return true;
  }

  // SkStrikeClient::DiscardableHandleManager implementation.
  bool deleteHandle(SkDiscardableHandleId handle_id) override {
    CHECK_LT(handle_id, next_handle_id_);
    return false;
  }

 private:
  SkDiscardableHandleId next_handle_id_ = 1u;
};

TestOptionsProvider::TestOptionsProvider()
    : discardable_manager_(sk_make_sp<DiscardableManager>()),
      strike_server_(discardable_manager_.get()),
      strike_client_(discardable_manager_),
      color_space_(SkColorSpace::MakeSRGB()) {
  serialize_options_.canvas = &canvas_;
  serialize_options_.image_provider = this;
  serialize_options_.transfer_cache = this;
  deserialize_options_.transfer_cache = this;
  deserialize_options_.strike_client = &strike_client_;
}

TestOptionsProvider::~TestOptionsProvider() = default;

void TestOptionsProvider::PushFonts() {
  std::vector<uint8_t> font_data;
  strike_server_.writeStrikeData(&font_data);
  if (font_data.size() == 0u)
    return;
  CHECK(strike_client_.readStrikeData(font_data.data(), font_data.size()));
}

ImageProvider::ScopedDecodedDrawImage TestOptionsProvider::GetDecodedDrawImage(
    const DrawImage& draw_image) {
  decoded_images_.push_back(draw_image);
  auto& entry = entry_map_[++transfer_cache_entry_id_];
  SkBitmap bitmap;
  const auto& paint_image = draw_image.paint_image();
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(paint_image.width(), paint_image.height()),
      SkBitmap::kZeroPixels_AllocFlag);
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  entry.set_image_for_testing(image);
  return ScopedDecodedDrawImage(DecodedDrawImage(
      transfer_cache_entry_id_, SkSize::MakeEmpty(), SkSize::Make(1u, 1u),
      draw_image.filter_quality(), true));
}

ServiceTransferCacheEntry* TestOptionsProvider::GetEntryInternal(
    TransferCacheEntryType entry_type,
    uint32_t entry_id) {
  if (entry_type != TransferCacheEntryType::kImage)
    return TransferCacheTestHelper::GetEntryInternal(entry_type, entry_id);
  auto it = entry_map_.find(entry_id);
  CHECK(it != entry_map_.end());
  return &it->second;
}

}  // namespace cc
