// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_DATA_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_DATA_H_
#pragma once

#include "base/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Wraps the state needed by the renderers.
struct TabRendererData {
  // Different types of network activity for a tab. The NetworkState of a tab
  // may be used to alter the UI (e.g. show different kinds of loading
  // animations).
  enum NetworkState {
    NETWORK_STATE_NONE,     // no network activity.
    NETWORK_STATE_WAITING,  // waiting for a connection.
    NETWORK_STATE_LOADING,  // connected, transferring data.
  };

  TabRendererData()
      : network_state(NETWORK_STATE_NONE),
        loading(false),
        crashed(false),
        off_the_record(false),
        show_icon(true),
        mini(false),
        blocked(false),
        phantom(false),
        app(false) {
  }

  SkBitmap favicon;
  NetworkState network_state;
  string16 title;
  bool loading;
  bool crashed;
  bool off_the_record;
  bool show_icon;
  bool mini;
  bool blocked;
  bool phantom;
  bool app;
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_DATA_H_
