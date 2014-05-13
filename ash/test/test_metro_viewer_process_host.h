// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_METRO_VIEWER_PROCESS_HOST_H_
#define ASH_TEST_TEST_METRO_VIEWER_PROCESS_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "win8/viewer/metro_viewer_process_host.h"

class AcceleratedSurface;

namespace ash {
namespace test {

class TestMetroViewerProcessHost : public win8::MetroViewerProcessHost {
 public:
  TestMetroViewerProcessHost(base::SingleThreadTaskRunner* ipc_task_runner);
  virtual ~TestMetroViewerProcessHost();

  bool closed_unexpectedly() { return closed_unexpectedly_; }

 private:
  // win8::MetroViewerProcessHost implementation
  virtual void OnChannelError() OVERRIDE;
  virtual void OnSetTargetSurface(gfx::NativeViewId target_surface,
                                  float device_scale) OVERRIDE;
  virtual void OnOpenURL(const base::string16& url) OVERRIDE;
  virtual void OnHandleSearchRequest(
      const base::string16& search_string) OVERRIDE;
  virtual void OnWindowSizeChanged(uint32 width, uint32 height) OVERRIDE;

  bool closed_unexpectedly_;

  DISALLOW_COPY_AND_ASSIGN(TestMetroViewerProcessHost);
};


}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_METRO_VIEWER_PROCESS_HOST_H_
