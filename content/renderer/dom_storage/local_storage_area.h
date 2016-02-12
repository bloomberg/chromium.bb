// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebStorageArea.h"
#include "url/origin.h"

namespace content {

class LocalStorageArea : public blink::WebStorageArea {
 public:
  explicit LocalStorageArea(const url::Origin& origin);
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
  size_t memoryBytesUsedByCache() const override;

 private:
  url::Origin origin_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageArea);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_
