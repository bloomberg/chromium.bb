// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_PREVIEW_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_PREVIEW_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_model_observer.h"

class Browser;
class InstantModel;

// Abstract base class for platform-specific Instant preview controllers.
class InstantPreviewController : public InstantModelObserver {
 protected:
  explicit InstantPreviewController(Browser* browser);
  virtual ~InstantPreviewController();

  Browser* const browser_;

 private:
  // Overridden from InstantModelObserver:
  virtual void PreviewStateChanged(const InstantModel& model) OVERRIDE = 0;
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_PREVIEW_CONTROLLER_H_
