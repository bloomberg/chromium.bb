// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bubble_anchor_helper.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#import "ui/base/cocoa/cocoa_base_utils.h"

bool HasVisibleLocationBarForBrowser(Browser* browser) {
  if (!browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return false;

  if (!browser->exclusive_access_manager()->context()->IsFullscreen())
    return true;

  return false;
}
