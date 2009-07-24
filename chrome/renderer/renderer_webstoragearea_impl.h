// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/string16.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebString.h"

class RendererWebStorageAreaImpl : public WebKit::WebStorageArea {
 public:
  RendererWebStorageAreaImpl(int64 namespace_id,
                             const WebKit::WebString& origin);
  virtual ~RendererWebStorageAreaImpl();

  // See WebStorageArea.h for documentation on these functions.
  virtual void lock(bool& invalidate_cache, size_t& bytes_left_in_quota);
  virtual void unlock();
  virtual unsigned length();
  virtual WebKit::WebString key(unsigned index, bool& key_exception);
  virtual WebKit::WebString getItem(const WebKit::WebString& key);
  virtual void setItem(const WebKit::WebString& key,
                       const WebKit::WebString& value,
                       bool& quota_exception);
  virtual void removeItem(const WebKit::WebString& key);
  virtual void clear();

 private:
  // Calls lock if we haven't already done so, and also gets a storage_area_id
  // if we haven't done so.  Fetches quota and cache invalidation information
  // while locking.  Only call on the WebKit thread.
  void EnsureInitializedAndLocked();

  // Update our quota calculation.  Returns true if we updated the quota.
  // Returns false if we couldn't update because we would have exceeded the
  // quota.  If an item is not in our cache, it'll require a getItem IPC in
  // order to determine the existing value's size.
  bool UpdateQuota(const WebKit::WebString& key,
                   const WebKit::WebString& value);

  // Set a key/value pair in our cache.
  void SetCache(const string16& key, const WebKit::WebString& value);

  // Used for initialization or storage_area_id_.
  int64 namespace_id_;
  string16 origin_;

  // The ID we use for all IPC.  Initialized lazily.
  int64 storage_area_id_;

  // storage_area_id_ should equal this iff its unitialized.
  static const int64 kUninitializedStorageAreaId = -1;

  // Do we currently hold the lock on this storage area?
  bool lock_held_;

  // We track how many bytes are left in the quota between lock and unlock
  // calls.  This allows us to avoid sync IPC on setItem.
  size_t bytes_left_in_quota_;

  // A cache of key/value pairs.  If the item exists, it's put in the
  // cached_items_ map.  If not, it's added to the cached_invalid_items_ set.
  // The lock IPC call tells us when to invalidate these caches.
  // TODO(jorlow): Instead of a map + a set, use NullableString16 once it's
  //               implemented: http://crbug.com/17343
  typedef base::hash_map<string16, string16> CacheMap;
  typedef base::hash_set<string16> CacheSet;
  CacheMap cached_items_;
  CacheSet cached_invalid_items_;
};

#endif  // CHROME_RENDERER_WEBSTORAGEAREA_IMPL_H_
