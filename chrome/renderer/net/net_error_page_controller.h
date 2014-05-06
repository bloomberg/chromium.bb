// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
#define CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/arguments.h"
#include "gin/wrappable.h"


namespace content {
class RenderFrame;
}

// This class makes various helper functions available to the
// error page loaded by NetErrorHelper.  It is bound to the JavaScript
// window.errorPageController object.
class NetErrorPageController
    : public gin::Wrappable<NetErrorPageController>,
      public content::RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(content::RenderFrame* render_frame);

 private:
  explicit NetErrorPageController(content::RenderFrame* render_frame);
  virtual ~NetErrorPageController();

  // Execute a "Load Stale" button click.
  bool LoadStaleButtonClick();

  // Execute a "Reload" button click.
  bool ReloadButtonClick();

  // Execute a "More" button click.
  bool MoreButtonClick();

  // Track a click when the page has suggestions from the navigation correction
  // service.
  bool TrackClick(const gin::Arguments& args);

  // gin::WrappableBase
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  // RenderFrameObserver.  Overridden to avoid being destroyed when RenderFrame
  // goes away; NetErrorPageController objects are owned by the JS
  // garbage collector.
  virtual void OnDestruct() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetErrorPageController);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
