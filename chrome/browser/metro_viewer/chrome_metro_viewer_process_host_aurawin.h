// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_
#define CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_

#include "win8/viewer/metro_viewer_process_host.h"

namespace base {
class FilePath;
}

class ChromeMetroViewerProcessHost : public win8::MetroViewerProcessHost {
 public:
  ChromeMetroViewerProcessHost();
  virtual ~ChromeMetroViewerProcessHost();

 private:
  // win8::MetroViewerProcessHost implementation
  virtual void OnChannelError() override;

  // IPC::Listener implementation
  virtual void OnChannelConnected(int32 peer_pid) override;
  virtual void OnSetTargetSurface(gfx::NativeViewId target_surface,
                                  float device_scale) override;
  virtual void OnOpenURL(const base::string16& url) override;
  virtual void OnHandleSearchRequest(
      const base::string16& search_string) override;
  virtual void OnWindowSizeChanged(uint32 width, uint32 height) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetroViewerProcessHost);
};

#endif  // CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_
