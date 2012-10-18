// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_PREVIEW_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_PREVIEW_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class InstantModel;

// Abstract base class for platform-specific Instant preview controllers.
class InstantPreviewController : public InstantModelObserver,
                                 public content::NotificationObserver {
 public:
  explicit InstantPreviewController(Browser* browser);
  virtual ~InstantPreviewController();

  // Overridden from InstantModelObserver:
  virtual void DisplayStateChanged(const InstantModel& model) OVERRIDE = 0;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  Browser* const browser_;

 private:
  void ResetInstant();

  content::NotificationRegistrar registrar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InstantPreviewController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_PREVIEW_CONTROLLER_H_
