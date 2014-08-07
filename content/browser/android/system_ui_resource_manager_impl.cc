// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/system_ui_resource_manager_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/threading/worker_pool.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "content/public/browser/android/ui_resource_client_android.h"
#include "content/public/browser/android/ui_resource_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

namespace content {

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
    DCHECK(bitmap_.empty());
    DCHECK(!bitmap.empty());
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
  base::Closure load_bitmap =
      base::Bind(&SystemUIResourceManagerImpl::LoadBitmap, type, bitmap);
  base::Closure finished_load =
      base::Bind(&SystemUIResourceManagerImpl::OnFinishedLoadBitmap,
                 weak_factory_.GetWeakPtr(),
                 type,
                 base::Owned(bitmap));
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE, load_bitmap, finished_load, true);
}

void SystemUIResourceManagerImpl::LoadBitmap(ResourceType type,
                                             SkBitmap* bitmap_holder) {
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
  }
  bitmap.setImmutable();
  *bitmap_holder = bitmap;
}

void SystemUIResourceManagerImpl::OnFinishedLoadBitmap(
    ResourceType type,
    SkBitmap* bitmap_holder) {
  DCHECK(bitmap_holder);
  GetEntry(type)->SetBitmap(*bitmap_holder);
}

}  // namespace content
