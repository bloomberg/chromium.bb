// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDERER_CLIENT_H_
#define CHROME_RENDERER_PRERENDER_PRERENDERER_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPrerendererClient.h"

namespace prerender {

class PrerendererClient : public content::RenderViewObserver,
                          public WebKit::WebPrerendererClient {
 public:
  explicit PrerendererClient(content::RenderView* render_view);

 private:
  virtual ~PrerendererClient();

  // Implements WebKit::WebPrerendererClient
  virtual void willAddPrerender(WebKit::WebPrerender* prerender) OVERRIDE;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDERER_CLIENT_H_

