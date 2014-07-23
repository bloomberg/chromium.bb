// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/android/thumbnail/thumbnail.h"
#include "content/public/browser/android/ui_resource_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace {

SkBitmap CreateSmallHolderBitmap() {
  SkBitmap small_bitmap;
  SkCanvas canvas(small_bitmap);
  small_bitmap.allocPixels(
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  canvas.drawColor(SK_ColorWHITE);
  small_bitmap.setImmutable();
  return small_bitmap;
}

}  // anonymous namespace

scoped_ptr<Thumbnail> Thumbnail::Create(
    TabId tab_id,
    const base::Time& time_stamp,
    float scale,
    content::UIResourceProvider* ui_resource_provider,
    ThumbnailDelegate* thumbnail_delegate) {
  return make_scoped_ptr(new Thumbnail(
      tab_id, time_stamp, scale, ui_resource_provider, thumbnail_delegate));
}

Thumbnail::Thumbnail(TabId tab_id,
                     const base::Time& time_stamp,
                     float scale,
                     content::UIResourceProvider* ui_resource_provider,
                     ThumbnailDelegate* thumbnail_delegate)
    : tab_id_(tab_id),
      time_stamp_(time_stamp),
      scale_(scale),
      bitmap_(gfx::Size(1, 1), true),
      ui_resource_id_(0),
      retrieved_(false),
      ui_resource_provider_(ui_resource_provider),
      thumbnail_delegate_(thumbnail_delegate) {
}

Thumbnail::~Thumbnail() {
  ClearUIResourceId();
}

void Thumbnail::SetBitmap(const SkBitmap& bitmap) {
  DCHECK(!bitmap.empty());
  ClearUIResourceId();
  scaled_content_size_ =
      gfx::ScaleSize(gfx::Size(bitmap.width(), bitmap.height()), 1.f / scale_);
  scaled_data_size_ = scaled_content_size_;
  bitmap_ = cc::UIResourceBitmap(bitmap);
}

void Thumbnail::SetCompressedBitmap(skia::RefPtr<SkPixelRef> compressed_bitmap,
                                    const gfx::Size& content_size) {
  DCHECK(compressed_bitmap);
  DCHECK(!content_size.IsEmpty());
  ClearUIResourceId();
  gfx::Size data_size(compressed_bitmap->info().width(),
                      compressed_bitmap->info().height());
  scaled_content_size_ = gfx::ScaleSize(content_size, 1.f / scale_);
  scaled_data_size_ = gfx::ScaleSize(data_size, 1.f / scale_);
  bitmap_ = cc::UIResourceBitmap(compressed_bitmap, data_size);
}

void Thumbnail::CreateUIResource() {
  DCHECK(ui_resource_provider_);
  if (!ui_resource_id_)
    ui_resource_id_ = ui_resource_provider_->CreateUIResource(this);
}

cc::UIResourceBitmap Thumbnail::GetBitmap(cc::UIResourceId uid,
                                          bool resource_lost) {
  if (retrieved_)
    return bitmap_;

  retrieved_ = true;

  cc::UIResourceBitmap old_bitmap(bitmap_);
  // Return a place holder for all other calls to GetBitmap.
  bitmap_ = cc::UIResourceBitmap(CreateSmallHolderBitmap());
  return old_bitmap;
}

void Thumbnail::UIResourceIsInvalid() {
  ui_resource_id_ = 0;
  if (thumbnail_delegate_)
    thumbnail_delegate_->InvalidateCachedThumbnail(this);
}

void Thumbnail::ClearUIResourceId() {
  if (ui_resource_id_ && ui_resource_provider_)
    ui_resource_provider_->DeleteUIResource(ui_resource_id_);
  ui_resource_id_ = 0;
}
