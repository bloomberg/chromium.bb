// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBIDBFACTORY_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBIDBFACTORY_IMPL_H_
#pragma once

#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

namespace WebKit {
class WebFrame;
class WebSecurityOrigin;
class WebString;
}

class RendererWebIDBFactoryImpl : public WebKit::WebIDBFactory {
 public:
  RendererWebIDBFactoryImpl();
  virtual ~RendererWebIDBFactoryImpl();

  // See WebIDBFactory.h for documentation on these functions.
  virtual void getDatabaseNames(
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebSecurityOrigin& origin,
      WebKit::WebFrame* web_frame,
      const WebKit::WebString& data_dir);

  virtual void open(
      const WebKit::WebString& name,
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebSecurityOrigin& origin,
      WebKit::WebFrame* web_frame,
      const WebKit::WebString& data_dir);
  virtual void deleteDatabase(
      const WebKit::WebString& name,
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebSecurityOrigin& origin,
      WebKit::WebFrame* web_frame,
      const WebKit::WebString& data_dir);
};

#endif  // CONTENT_RENDERER_RENDERER_WEBIDBFACTORY_IMPL_H_
