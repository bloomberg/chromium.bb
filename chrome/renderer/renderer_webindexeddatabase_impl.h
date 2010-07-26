// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBINDEXEDDATABASE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBINDEXEDDATABASE_IMPL_H_
#pragma once

#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIndexedDatabase.h"

namespace WebKit {
class WebFrame;
class WebIDBDatabase;
class WebSecurityOrigin;
class WebString;
}

class RendererWebIndexedDatabaseImpl : public WebKit::WebIndexedDatabase {
 public:
  RendererWebIndexedDatabaseImpl();
  virtual ~RendererWebIndexedDatabaseImpl();

  // See WebIndexedDatabase.h for documentation on these functions.
  virtual void open(
      const WebKit::WebString& name, const WebKit::WebString& description,
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebSecurityOrigin& origin, WebKit::WebFrame* web_frame);
};

#endif  // CHROME_RENDERER_RENDERER_WEBINDEXEDDATABASE_IMPL_H_
