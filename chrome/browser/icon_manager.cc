// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace {

void RunCallbackIfNotCanceled(
    const CancelableTaskTracker::IsCanceledCallback& is_canceled,
    const IconManager::IconRequestCallback& callback,
    gfx::Image* image) {
  if (is_canceled.Run())
    return;
  callback.Run(image);
}

}  // namespace

struct IconManager::ClientRequest {
  IconRequestCallback callback;
  IconGroupID group;
  IconLoader::IconSize size;
};

IconManager::IconManager() {
}

IconManager::~IconManager() {
  STLDeleteValues(&icon_cache_);
}

gfx::Image* IconManager::LookupIcon(const base::FilePath& file_name,
                                    IconLoader::IconSize size) {
  IconGroupID group = GetGroupIDFromFilepath(file_name);
  IconMap::iterator it = icon_cache_.find(CacheKey(group, size));
  if (it != icon_cache_.end())
    return it->second;

  return NULL;
}

CancelableTaskTracker::TaskId IconManager::LoadIcon(
    const base::FilePath& file_name,
    IconLoader::IconSize size,
    const IconRequestCallback& callback,
    CancelableTaskTracker* tracker) {
  IconGroupID group = GetGroupIDFromFilepath(file_name);

  IconLoader* loader = new IconLoader(group, size, this);
  loader->AddRef();
  loader->Start();

  CancelableTaskTracker::IsCanceledCallback is_canceled;
  CancelableTaskTracker::TaskId id = tracker->NewTrackedTaskId(&is_canceled);
  IconRequestCallback callback_runner = base::Bind(
      &RunCallbackIfNotCanceled, is_canceled, callback);

  ClientRequest client_request = { callback_runner, group, size };
  requests_[loader] = client_request;
  return id;
}

// IconLoader::Delegate implementation -----------------------------------------

bool IconManager::OnImageLoaded(IconLoader* loader, gfx::Image* result) {
  ClientRequests::iterator rit = requests_.find(loader);

  // Balances the AddRef() in LoadIcon().
  loader->Release();

  // Look up our client state.
  if (rit == requests_.end()) {
    NOTREACHED();
    return false;  // Return false to indicate result should be deleted.
  }

  const ClientRequest& client_request = rit->second;

  // Cache the bitmap. Watch out: |result| or the cached bitmap may be NULL to
  // indicate a current or past failure.
  CacheKey key(client_request.group, client_request.size);
  IconMap::iterator it = icon_cache_.find(key);
  if (it != icon_cache_.end() && result && it->second) {
    it->second->SwapRepresentations(result);
    delete result;
    result = it->second;
  } else {
    icon_cache_[key] = result;
  }

  // Inform our client that the request has completed.
  client_request.callback.Run(result);
  requests_.erase(rit);

  return true;  // Indicates we took ownership of result.
}

IconManager::CacheKey::CacheKey(const IconGroupID& group,
                                IconLoader::IconSize size)
    : group(group),
      size(size) {
}

bool IconManager::CacheKey::operator<(const CacheKey &other) const {
  if (group != other.group)
    return group < other.group;
  return size < other.size;
}
