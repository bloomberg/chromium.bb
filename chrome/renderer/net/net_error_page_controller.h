// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
#define CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_

#include "base/macros.h"
#include "components/error_page/renderer/net_error_helper_core.h"
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
  ~NetErrorPageController() override;

  // Execute a "Show saved copy" button click.
  bool ShowSavedCopyButtonClick();

  // Execute a "Reload" button click.
  bool ReloadButtonClick();

  // Execute a "Details" button click.
  bool DetailsButtonClick();

  // Track easter egg plays.
  bool TrackEasterEgg();

  // Track "Show cached copy/page" button clicks.
  bool TrackCachedCopyButtonClick(bool is_default_label);

  // Track a click when the page has suggestions from the navigation correction
  // service.
  bool TrackClick(const gin::Arguments& args);

  // Used internally by other button click methods.
  bool ButtonClick(error_page::NetErrorHelperCore::Button button);

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // RenderFrameObserver.  Overridden to avoid being destroyed when RenderFrame
  // goes away; NetErrorPageController objects are owned by the JS
  // garbage collector.
  void OnDestruct() override;

  DISALLOW_COPY_AND_ASSIGN(NetErrorPageController);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
