// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_VIEWS_UI_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_VIEWS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/media_router/media_router_ui_base.h"

namespace media_router {

// Functions as an intermediary between MediaRouter and Views Cast dialog.
class MediaRouterViewsUI : public MediaRouterUIBase {
 public:
  MediaRouterViewsUI();
  ~MediaRouterViewsUI() override;

 private:
  // MediaRouterUIBase:
  void UpdateSinks() override;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterViewsUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_VIEWS_UI_H_
