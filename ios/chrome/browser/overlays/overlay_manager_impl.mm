// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/overlay_manager_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Factory method

// static
OverlayManager* OverlayManager::FromBrowser(Browser* browser,
                                            OverlayModality modality) {
  OverlayManagerImpl::Container::CreateForUserData(browser, browser);
  return OverlayManagerImpl::Container::FromUserData(browser)
      ->ManagerForModality(modality);
}

#pragma mark - OverlayManagerImpl::Container

OVERLAY_USER_DATA_SETUP_IMPL(OverlayManagerImpl::Container);

OverlayManagerImpl::Container::Container(Browser* browser) : browser_(browser) {
  DCHECK(browser_);
}

OverlayManagerImpl::Container::~Container() = default;

OverlayManagerImpl* OverlayManagerImpl::Container::ManagerForModality(
    OverlayModality modality) {
  auto& manager = managers_[modality];
  if (!manager) {
    manager = base::WrapUnique(new OverlayManagerImpl(browser_, modality));
  }
  return manager.get();
}

#pragma mark - OverlayManagerImpl

OverlayManagerImpl::OverlayManagerImpl(Browser* browser,
                                       OverlayModality modality)
    : presenter_(modality, browser->GetWebStateList()) {
  browser->AddObserver(this);
}

OverlayManagerImpl::~OverlayManagerImpl() = default;

#pragma mark BrowserObserver

void OverlayManagerImpl::BrowserDestroyed(Browser* browser) {
  presenter_.Disconnect();
  browser->RemoveObserver(this);
}

#pragma mark OverlayManager

void OverlayManagerImpl::SetUIDelegate(OverlayUIDelegate* ui_delegate) {
  presenter_.SetUIDelegate(ui_delegate);
}

void OverlayManagerImpl::AddObserver(OverlayManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void OverlayManagerImpl::RemoveObserver(OverlayManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}
