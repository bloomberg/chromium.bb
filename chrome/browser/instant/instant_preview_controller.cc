// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_preview_controller.h"

#include "chrome/browser/instant/instant_model.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

InstantPreviewController::InstantPreviewController(Browser* browser)
    : browser_(browser) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_INSTANT_RESET,
                 content::NotificationService::AllSources());
  ResetInstant();
}

InstantPreviewController::~InstantPreviewController() {
  InstantController* instant = browser_->instant_controller()->instant();
  if (instant)
    instant->model()->RemoveObserver(this);
}

void InstantPreviewController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_INSTANT_RESET, type);
  ResetInstant();
}

void InstantPreviewController::ResetInstant() {
  InstantController* instant = browser_->instant_controller()->instant();
  if (instant && !instant->model()->HasObserver(this))
    instant->model()->AddObserver(this);
}
