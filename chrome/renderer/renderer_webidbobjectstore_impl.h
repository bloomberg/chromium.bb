// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBOBJECTSTORE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBOBJECTSTORE_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBObjectStore.h"

namespace WebKit {
class WebFrame;
class WebIDBCallbacks;
class WebString;
}

class RendererWebIDBObjectStoreImpl : public WebKit::WebIDBObjectStore {
 public:
  explicit RendererWebIDBObjectStoreImpl(int32 idb_object_store_id);
  virtual ~RendererWebIDBObjectStoreImpl();

  // WebKit::WebIDBObjectStore
  virtual WebKit::WebString name() const;
  virtual WebKit::WebString keyPath() const;

 private:
  int32 idb_object_store_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBOBJECTSTORE_IMPL_H_
