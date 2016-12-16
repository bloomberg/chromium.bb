// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/task_runner.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace {

void RunCallbackIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    const IconManager::IconRequestCallback& callback,
    gfx::Image* image) {
  if (is_canceled.Run())
    return;
  callback.Run(image);
}

}  // namespace

struct IconManager::ClientRequest {
  IconRequestCallback callback;
  base::FilePath file_path;
  IconLoader::IconSize size;
};

IconManager::IconManager() {
}

IconManager::~IconManager() {
}

gfx::Image* IconManager::LookupIconFromFilepath(const base::FilePath& file_path,
                                                IconLoader::IconSize size) {
  auto group_it = group_cache_.find(file_path);
  if (group_it == group_cache_.end())
    return nullptr;

  CacheKey key(group_it->second, size);
  auto icon_it = icon_cache_.find(key);
  if (icon_it == icon_cache_.end())
    return nullptr;

  return icon_it->second.get();
}

base::CancelableTaskTracker::TaskId IconManager::LoadIcon(
    const base::FilePath& file_path,
    IconLoader::IconSize size,
    const IconRequestCallback& callback,
    base::CancelableTaskTracker* tracker) {
  IconLoader* loader = new IconLoader(file_path, size, this);
  loader->AddRef();
  loader->Start();

  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);
  IconRequestCallback callback_runner = base::Bind(
      &RunCallbackIfNotCanceled, is_canceled, callback);

  requests_[loader] = {callback_runner, file_path, size};
  return id;
}

// IconLoader::Delegate implementation -----------------------------------------

void IconManager::OnImageLoaded(IconLoader* loader,
                                std::unique_ptr<gfx::Image> result,
                                const IconLoader::IconGroup& group) {
  auto request_it = requests_.find(loader);
  DCHECK(request_it != requests_.end());

  // Balances the AddRef() in LoadIcon().
  loader->Release();

  const ClientRequest& client_request = request_it->second;

  // Cache the bitmap. Watch out: |result| may be null, which indicates a
  // failure. We assume that if we have an entry in |icon_cache_| it must not be
  // null.
  CacheKey key(group, client_request.size);
  if (result) {
    client_request.callback.Run(result.get());
    icon_cache_[key] = std::move(result);
  } else {
    client_request.callback.Run(nullptr);
    icon_cache_.erase(key);
  }

  group_cache_[client_request.file_path] = group;

  requests_.erase(request_it);
}

IconManager::CacheKey::CacheKey(const IconLoader::IconGroup& group,
                                IconLoader::IconSize size)
    : group(group), size(size) {}

bool IconManager::CacheKey::operator<(const CacheKey &other) const {
  return std::tie(group, size) < std::tie(other.group, other.size);
}
