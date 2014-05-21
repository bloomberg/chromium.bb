// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_
#define CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_

#include "win8/viewer/metro_viewer_process_host.h"

namespace base {
class FilePath;
}

// Handles the activate desktop command for Metro Chrome Ash.   The |ash_exit|
// parameter indicates whether the Ash process would be shutdown after
// activating the desktop.
void HandleActivateDesktop(const base::FilePath& shortcut, bool ash_exit);

class ChromeMetroViewerProcessHost : public win8::MetroViewerProcessHost {
 public:
  ChromeMetroViewerProcessHost();
  virtual ~ChromeMetroViewerProcessHost();

  // Returns the singleton ChromeMetroViewerProcessHost instance. This may
  // return NULL.
  static ChromeMetroViewerProcessHost* instance() {
    return instance_;
  }

 private:
  // win8::MetroViewerProcessHost implementation
  virtual void OnChannelError() OVERRIDE;
  // IPC::Listener implementation
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnSetTargetSurface(gfx::NativeViewId target_surface,
                                  float device_scale) OVERRIDE;
  virtual void OnOpenURL(const base::string16& url) OVERRIDE;
  virtual void OnHandleSearchRequest(
      const base::string16& search_string) OVERRIDE;
  virtual void OnWindowSizeChanged(uint32 width, uint32 height) OVERRIDE;

  static ChromeMetroViewerProcessHost* instance_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetroViewerProcessHost);
};

#endif  // CHROME_BROWSER_METRO_VIEWER_CHROME_METRO_VIEWER_PROCESS_HOST_AURAWIN_H_
