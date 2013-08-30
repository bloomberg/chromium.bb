// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_
#define CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_

#include "win8/viewer/metro_viewer_process_host.h"

class ChromeMetroViewerProcessHost : public win8::MetroViewerProcessHost {
 public:
  ChromeMetroViewerProcessHost();

 private:
  // win8::MetroViewerProcessHost implementation
  virtual void OnChannelError() OVERRIDE;
  // IPC::Listener implementation
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnSetTargetSurface(gfx::NativeViewId target_surface) OVERRIDE;
  virtual void OnOpenURL(const string16& url) OVERRIDE;
  virtual void OnHandleSearchRequest(const string16& search_string) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetroViewerProcessHost);
};

#endif  // CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_
