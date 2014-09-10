// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/system_ui_resource_manager_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/threading/worker_pool.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "content/public/browser/android/ui_resource_client_android.h"
#include "content/public/browser/android/ui_resource_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkPorterDuff.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/screen.h"

namespace content {
namespace {

SkBitmap CreateOverscrollGlowLBitmap(const gfx::Size& screen_size) {
  const float kSin = 0.5f;    // sin(PI / 6)
  const float kCos = 0.866f;  // cos(PI / 6);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setAlpha(0xBB);
  paint.setStyle(SkPaint::kFill_Style);

  const float arc_width =
      std::min(screen_size.width(), screen_size.height()) * 0.5f / kSin;
  const float y = kCos * arc_width;
  const float height = arc_width - y;
  gfx::Size bounds(arc_width, height);
  SkRect arc_rect = SkRect::MakeXYWH(
      -arc_width / 2.f, -arc_width - y, arc_width * 2.f, arc_width * 2.f);
  SkBitmap glow_bitmap;
  glow_bitmap.allocPixels(SkImageInfo::MakeA8(bounds.width(), bounds.height()));
  glow_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(glow_bitmap);
  canvas.clipRect(SkRect::MakeXYWH(0, 0, bounds.width(), bounds.height()));
  canvas.drawArc(arc_rect, 45, 90, true, paint);
  return glow_bitmap;
}

void LoadBitmap(ui::SystemUIResourceManager::ResourceType type,
                SkBitmap* bitmap_holder,
                const gfx::Size& screen_size) {
  TRACE_EVENT1(
      "browser", "SystemUIResourceManagerImpl::LoadBitmap", "type", type);
  SkBitmap bitmap;
  switch (type) {
    case ui::SystemUIResourceManager::OVERSCROLL_EDGE:
      bitmap = gfx::CreateSkBitmapFromAndroidResource(
          "android:drawable/overscroll_edge", gfx::Size(128, 12));
      break;
    case ui::SystemUIResourceManager::OVERSCROLL_GLOW:
      bitmap = gfx::CreateSkBitmapFromAndroidResource(
          "android:drawable/overscroll_glow", gfx::Size(128, 64));
      break;
    case ui::SystemUIResourceManager::OVERSCROLL_GLOW_L:
      bitmap = CreateOverscrollGlowLBitmap(screen_size);
      break;
  }
  bitmap.setImmutable();
  *bitmap_holder = bitmap;
}

}  // namespace

class SystemUIResourceManagerImpl::Entry
    : public content::UIResourceClientAndroid {
 public:
  explicit Entry(UIResourceProvider* provider) : id_(0), provider_(provider) {
    DCHECK(provider);
  }

  virtual ~Entry() {
    if (id_)
      provider_->DeleteUIResource(id_);
    id_ = 0;
  }

  void SetBitmap(const SkBitmap& bitmap) {
    DCHECK(!bitmap.empty());
    DCHECK(bitmap_.empty());
    DCHECK(!id_);
    bitmap_ = bitmap;
  }

  cc::UIResourceId GetUIResourceId() {
    if (bitmap_.empty())
      return 0;
    if (!id_)
      id_ = provider_->CreateUIResource(this);
    return id_;
  }

  // content::UIResourceClient implementation.
  virtual cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                         bool resource_lost) OVERRIDE {
    DCHECK(!bitmap_.empty());
    return cc::UIResourceBitmap(bitmap_);
  }

  // content::UIResourceClientAndroid implementation.
  virtual void UIResourceIsInvalid() OVERRIDE { id_ = 0; }

  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  SkBitmap bitmap_;
  cc::UIResourceId id_;
  UIResourceProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

SystemUIResourceManagerImpl::SystemUIResourceManagerImpl(
    UIResourceProvider* ui_resource_provider)
    : ui_resource_provider_(ui_resource_provider), weak_factory_(this) {
}

SystemUIResourceManagerImpl::~SystemUIResourceManagerImpl() {
}

void SystemUIResourceManagerImpl::PreloadResource(ResourceType type) {
  GetEntry(type);
}

cc::UIResourceId SystemUIResourceManagerImpl::GetUIResourceId(
    ResourceType type) {
  return GetEntry(type)->GetUIResourceId();
}

SystemUIResourceManagerImpl::Entry* SystemUIResourceManagerImpl::GetEntry(
    ResourceType type) {
  DCHECK_GE(type, RESOURCE_TYPE_FIRST);
  DCHECK_LE(type, RESOURCE_TYPE_LAST);
  if (!resource_map_[type]) {
    resource_map_[type].reset(new Entry(ui_resource_provider_));
    // Lazily build the resource.
    BuildResource(type);
  }
  return resource_map_[type].get();
}

void SystemUIResourceManagerImpl::BuildResource(ResourceType type) {
  DCHECK(GetEntry(type)->bitmap().empty());

  // Instead of blocking the main thread, we post a task to load the bitmap.
  SkBitmap* bitmap = new SkBitmap();
  gfx::Size screen_size =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().GetSizeInPixel();
  base::Closure load_bitmap =
      base::Bind(&LoadBitmap, type, bitmap, screen_size);
  base::Closure finished_load =
      base::Bind(&SystemUIResourceManagerImpl::OnFinishedLoadBitmap,
                 weak_factory_.GetWeakPtr(),
                 type,
                 base::Owned(bitmap));
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE, load_bitmap, finished_load, true);
}

void SystemUIResourceManagerImpl::OnFinishedLoadBitmap(
    ResourceType type,
    SkBitmap* bitmap_holder) {
  DCHECK(bitmap_holder);
  GetEntry(type)->SetBitmap(*bitmap_holder);
}

}  // namespace content
