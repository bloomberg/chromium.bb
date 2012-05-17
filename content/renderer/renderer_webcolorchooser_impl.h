// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebColorChooser.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebColorChooserClient.h"
#include "third_party/skia/include/core/SkColor.h"

namespace WebKit {
class WebFrame;
}

class RenderViewImpl;

class RendererWebColorChooserImpl : public WebKit::WebColorChooser,
                                    public content::RenderViewObserver {
 public:
  explicit RendererWebColorChooserImpl(RenderViewImpl* sender,
                                       WebKit::WebColorChooserClient*);
  virtual ~RendererWebColorChooserImpl();

  virtual void setSelectedColor(const WebKit::WebColor);
  virtual void endChooser();

  void Open(SkColor initial_color);

  WebKit::WebColorChooserClient* client() { return client_; }

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDidChooseColorResponse(int color_chooser_id, SkColor color);
  void OnDidEndColorChooser(int color_chooser_id);

  int identifier_;
  WebKit::WebColorChooserClient* client_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebColorChooserImpl);
};

#endif  // CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
