// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_ERROR_CACHE_LOAD_H_
#define CHROME_RENDERER_NET_ERROR_CACHE_LOAD_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"
#include "url/gurl.h"

// ErrorCacheLoad class:
//
// This class makes cache loading operations available to the
// error page loaded by NetErrorHelper.  It is bound to the javascript
// window.errorCacheLoad object.

class GURL;

namespace content {
class RenderFrame;
}

class ErrorCacheLoad : public gin::Wrappable<ErrorCacheLoad>,
                       public content::RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(content::RenderFrame* render_frame, const GURL& page_url);

 private:
  ErrorCacheLoad(content::RenderFrame* render_frame, const GURL& page_url);
  virtual ~ErrorCacheLoad();

  // Loads the original URL associated with the frame, with the blink
  // ReturnCacheDataDontLoad flag set to make sure that the value is
  // only gotten from cache.
  bool ReloadStaleInstance();

  // gin::WrappableBase
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  // RenderFrameObserver.  Overridden to avoid being destroyed when RenderFrame
  // goes away; ErrorCacheLoad objects are owned by the JS garbage collector.
  virtual void OnDestruct() OVERRIDE;

  // We'll be torn down by V8 when the page goes away, so it's safe to hold
  // a naked pointer to the render frame.
  content::RenderFrame* render_frame_;

  const GURL page_url_;

  bool render_frame_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(ErrorCacheLoad);
};

#endif  // CHROME_RENDERER_NET_ERROR_CACHE_LOAD_H_
