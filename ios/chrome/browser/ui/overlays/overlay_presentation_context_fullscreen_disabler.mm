// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_fullscreen_disabler.h"

#include "base/check.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - OverlayContainerFullscreenDisabler

OverlayContainerFullscreenDisabler::OverlayContainerFullscreenDisabler(
    Browser* browser,
    OverlayModality modality)
    : fullscreen_disabler_(
          fullscreen::features::ShouldScopeFullscreenControllerToBrowser()
              ? FullscreenController::FromBrowser(browser)
              : FullscreenController::FromBrowserState(
                    browser->GetBrowserState()),
          OverlayPresenter::FromBrowser(browser, modality)) {}

OverlayContainerFullscreenDisabler::~OverlayContainerFullscreenDisabler() =
    default;

#pragma mark - OverlayContainerFullscreenDisabler::FullscreenDisabler

OverlayContainerFullscreenDisabler::FullscreenDisabler::FullscreenDisabler(
    FullscreenController* fullscreen_controller,
    OverlayPresenter* overlay_presenter)
    : fullscreen_controller_(fullscreen_controller), scoped_observer_(this) {
  DCHECK(fullscreen_controller_);
  DCHECK(overlay_presenter);
  scoped_observer_.Add(overlay_presenter);
}

OverlayContainerFullscreenDisabler::FullscreenDisabler::~FullscreenDisabler() =
    default;

void OverlayContainerFullscreenDisabler::FullscreenDisabler::WillShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  disabler_ = std::make_unique<AnimatedScopedFullscreenDisabler>(
      fullscreen_controller_);
  disabler_->StartAnimation();
}

void OverlayContainerFullscreenDisabler::FullscreenDisabler::DidHideOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  disabler_ = nullptr;
}

void OverlayContainerFullscreenDisabler::FullscreenDisabler::
    OverlayPresenterDestroyed(OverlayPresenter* presenter) {
  scoped_observer_.Remove(presenter);
  disabler_ = nullptr;
}
