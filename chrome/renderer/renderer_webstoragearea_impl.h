// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebString.h"

class RendererWebStorageAreaImpl : public WebKit::WebStorageArea {
 public:
  RendererWebStorageAreaImpl(int64 namespace_id,
                             const WebKit::WebString& origin);
  virtual ~RendererWebStorageAreaImpl();

  // See WebStorageArea.h for documentation on these functions.
  virtual unsigned length();
  virtual WebKit::WebString key(unsigned index);
  virtual WebKit::WebString getItem(const WebKit::WebString& key);
  virtual void setItem(
      const WebKit::WebString& key, const WebKit::WebString& value,
      const WebKit::WebURL& url, bool& quota_exception);
  virtual void removeItem(const WebKit::WebString& key,
                          const WebKit::WebURL& url);
  virtual void clear(const WebKit::WebURL& url);

 private:
  // The ID we use for all IPC.
  int64 storage_area_id_;
};

#endif  // CHROME_RENDERER_WEBSTORAGEAREA_IMPL_H_
