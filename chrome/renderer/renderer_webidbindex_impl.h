// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBINDEX_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBINDEX_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBIndex.h"

class RendererWebIDBIndexImpl : public WebKit::WebIDBIndex {
 public:
  explicit RendererWebIDBIndexImpl(int32 idb_index_id);
  virtual ~RendererWebIDBIndexImpl();

  // WebKit::WebIDBIndex
  virtual WebKit::WebString name() const;
  virtual WebKit::WebString storeName() const;
  virtual WebKit::WebString keyPath() const;
  virtual bool unique() const;
  virtual void openObjectCursor(const WebKit::WebIDBKeyRange& range,
                                unsigned short direction,
                                WebKit::WebIDBCallbacks* callbacks,
                                const WebKit::WebIDBTransaction& transaction);
  virtual void openCursor(const WebKit::WebIDBKeyRange& range,
                          unsigned short direction,
                          WebKit::WebIDBCallbacks* callbacks,
                          const WebKit::WebIDBTransaction& transaction);
  virtual void getObject(const WebKit::WebIDBKey& key,
                         WebKit::WebIDBCallbacks* callbacks,
                         const WebKit::WebIDBTransaction& transaction);
  virtual void get(const WebKit::WebIDBKey& key,
                   WebKit::WebIDBCallbacks* callbacks,
                   const WebKit::WebIDBTransaction& transaction);

 private:
  int32 idb_index_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBINDEX_IMPL_H_
