// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_THREE_D_API_OBSERVER_H_
#define CHROME_BROWSER_GPU_THREE_D_API_OBSERVER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/gpu_data_manager_observer.h"

class ThreeDAPIObserver : public content::GpuDataManagerObserver {
 public:
  ThreeDAPIObserver();
  virtual ~ThreeDAPIObserver();

 private:
  virtual void DidBlock3DAPIs(const GURL& url,
                              int render_process_id,
                              int render_view_id,
                              content::ThreeDAPIType requester) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ThreeDAPIObserver);
};

#endif  // CHROME_BROWSER_GPU_THREE_D_API_OBSERVER_H_
