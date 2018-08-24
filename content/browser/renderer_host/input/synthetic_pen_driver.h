// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PEN_DRIVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PEN_DRIVER_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_mouse_driver.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT SyntheticPenDriver : public SyntheticMouseDriver {
 public:
  SyntheticPenDriver();
  ~SyntheticPenDriver() override;

  void Leave(int index = 0) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticPenDriver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PEN_DRIVER_H_
