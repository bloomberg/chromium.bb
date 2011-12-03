// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

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
      const WebKit::WebURL& url, WebStorageArea::Result& result,
      WebKit::WebString& old_value);
  virtual void removeItem(
      const WebKit::WebString& key, const WebKit::WebURL& url,
      WebKit::WebString& old_value);
  virtual void clear(const WebKit::WebURL& url, bool& cleared_something);

 private:
  // The ID we use for all IPC.
  int64 storage_area_id_;
};

#endif  // CONTENT_RENDERER_RENDERER_WEBSTORAGEAREA_IMPL_H_
