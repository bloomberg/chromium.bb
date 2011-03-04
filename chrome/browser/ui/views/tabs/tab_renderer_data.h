// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_RENDERER_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_RENDERER_DATA_H_
#pragma once

#include "base/process_util.h"
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

  TabRendererData();
  ~TabRendererData();

  // This interprets the crashed status to decide whether or not this
  // render data represents a tab that is "crashed" (i.e. the render
  // process died unexpectedly).
  bool IsCrashed() const {
    return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
            crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
            crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION);
  }

  // Returns true if the TabRendererData is same as given |data|. Two favicons
  // are considered equals if two SkBitmaps point to the same SkPixelRef object.
  bool Equals(const TabRendererData& data);

  SkBitmap favicon;
  NetworkState network_state;
  string16 title;
  bool loading;
  base::TerminationStatus crashed_status;
  bool off_the_record;
  bool show_icon;
  bool mini;
  bool blocked;
  bool app;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_RENDERER_DATA_H_
