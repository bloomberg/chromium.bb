// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREA_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREA_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class LocalStorageCachedAreas;
class StoragePartitionService;

// An in-process implementation of LocalStorage using a LevelDB Mojo service.
// Maintains a complete cache of the origin's Map of key/value pairs for fast
// access. The cache is primed on first access and changes are written to the
// backend through the level db interface pointer. Mutations originating in
// other processes are applied to the cache via LevelDBObserver callbacks.
class LocalStorageCachedArea : public LevelDBObserver,
                               public base::RefCounted<LocalStorageCachedArea> {
 public:
  LocalStorageCachedArea(const url::Origin& origin,
                         StoragePartitionService* storage_partition_service,
                         LocalStorageCachedAreas* cached_areas);

  // These correspond to blink::WebStorageArea.
  unsigned GetLength();
  base::NullableString16 GetKey(unsigned index);
  base::NullableString16 GetItem(const base::string16& key);
  bool SetItem(const base::string16& key,
               const base::string16& value,
               const GURL& page_url);
  void RemoveItem(const base::string16& key,
                  const GURL& page_url);
  void Clear(const GURL& page_url);

  const url::Origin& origin() { return origin_; }

 private:
  friend class base::RefCounted<LocalStorageCachedArea>;
  ~LocalStorageCachedArea() override;

  // LevelDBObserver:
  void KeyChanged(mojo::Array<uint8_t> key,
                  mojo::Array<uint8_t> new_value,
                  mojo::Array<uint8_t> old_value,
                  const mojo::String& source) override;
  void KeyDeleted(mojo::Array<uint8_t> key,
                  const mojo::String& source) override;
  void AllDeleted(const mojo::String& source) override;

  // Synchronously fetches the origin's local storage data if it hasn't been
  // fetched already.
  void EnsureLoaded();

  bool loaded_;
  url::Origin origin_;
  LevelDBWrapperPtr leveldb_;
  mojo::Binding<LevelDBObserver> binding_;
  LocalStorageCachedAreas* cached_areas_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCachedArea);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREA_H_
