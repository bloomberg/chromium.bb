// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include "chrome/renderer/render_widget_fullscreen.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWidget.h"

namespace pepper {
class PluginInstance;
class FullscreenContainer;
}

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidgetFullscreen {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32 opener_id,
      RenderThreadBase* render_thread,
      pepper::PluginInstance* plugin);

  // Asks the browser to close this view, which will tear off the window and
  // close this widget.
  void SendClose();

  // Invalidate the whole widget to force a redraw.
  void GenerateFullRepaint();

  pepper::FullscreenContainer* container() const { return container_.get(); }

 protected:
  RenderWidgetFullscreenPepper(RenderThreadBase* render_thread,
                               pepper::PluginInstance* plugin);
  virtual ~RenderWidgetFullscreenPepper();

  // RenderWidget API.
  virtual void DidInitiatePaint();
  virtual void DidFlushPaint();
  virtual void Close();

  // RenderWidgetFullscreen API.
  virtual WebKit::WebWidget* CreateWebWidget();

 private:
  // The plugin instance this widget wraps.
  pepper::PluginInstance* plugin_;

  // The FullscreenContainer that the plugin instance can callback into.
  scoped_ptr<pepper::FullscreenContainer> container_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

#endif  // CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
