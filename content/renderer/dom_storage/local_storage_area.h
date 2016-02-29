// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_

#include "base/macros.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/WebStorageArea.h"
#include "url/origin.h"

namespace content {
class StoragePartitionService;

// Maintains a complete cache of the origin's Map of key/value pairs for fast
// access. The cache is primed on first access and changes are written to the
// backend through the level db interface pointer. Mutations originating in
// other processes are applied to the cache via the ApplyMutation method.
class LocalStorageArea : public blink::WebStorageArea,
                         public LevelDBObserver {
 public:
  LocalStorageArea(const url::Origin& origin,
                   StoragePartitionService* storage_partition_service);
  ~LocalStorageArea() override;

  // blink::WebStorageArea.h:
  unsigned length() override;
  blink::WebString key(unsigned index) override;
  blink::WebString getItem(const blink::WebString& key) override;
  void setItem(const blink::WebString& key,
               const blink::WebString& value,
               const blink::WebURL& page_url,
               WebStorageArea::Result& result) override;
  void removeItem(const blink::WebString& key,
                  const blink::WebURL& page_url) override;
  void clear(const blink::WebURL& url) override;

  // LevelDBObserver:
  void KeyChanged(mojo::Array<uint8_t> key,
                  mojo::Array<uint8_t> new_value,
                  mojo::Array<uint8_t> old_value,
                  const mojo::String& source) override;
  void KeyDeleted(mojo::Array<uint8_t> key,
                  const mojo::String& source) override;
  void AllDeleted(const mojo::String& source) override;

 private:
  url::Origin origin_;
  LevelDBWrapperPtr leveldb_;
  mojo::Binding<LevelDBObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageArea);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_
