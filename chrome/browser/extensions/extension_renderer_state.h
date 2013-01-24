// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_RENDERER_STATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_RENDERER_STATE_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/memory/singleton.h"

// This class keeps track of renderer state for use on the IO thread. All
// methods should be called on the IO thread except for Init and Shutdown.
class ExtensionRendererState {
 public:
  static ExtensionRendererState* GetInstance();

  // These are called on the UI thread to start and stop listening to tab
  // notifications.
  void Init();
  void Shutdown();

  // Looks up the tab and window ID for a given render view. Returns true
  // if we have the IDs in our map. Called on the IO thread.
  bool GetTabAndWindowId(
      int render_process_host_id, int routing_id, int* tab_id, int* window_id);

 private:
  class TabObserver;
  friend class TabObserver;
  friend struct DefaultSingletonTraits<ExtensionRendererState>;

  typedef std::pair<int, int> RenderId;
  typedef std::pair<int, int> TabAndWindowId;
  typedef std::map<RenderId, TabAndWindowId> TabAndWindowIdMap;

  ExtensionRendererState();
  ~ExtensionRendererState();

  // Adds or removes a render view from our map.
  void SetTabAndWindowId(
      int render_process_host_id, int routing_id, int tab_id, int window_id);
  void ClearTabAndWindowId(
      int render_process_host_id, int routing_id);

  TabObserver* observer_;
  TabAndWindowIdMap map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRendererState);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_RENDERER_STATE_H_
