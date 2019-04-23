// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/overlay_manager_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Factory method

// static
OverlayManager* OverlayManager::FromBrowser(Browser* browser,
                                            OverlayModality modality) {
  OverlayManagerImpl::Container::CreateForUserData(browser,
                                                   browser->GetWebStateList());
  return OverlayManagerImpl::Container::FromUserData(browser)
      ->ManagerForModality(modality);
}

#pragma mark - OverlayManagerImpl::Container

OVERLAY_USER_DATA_SETUP_IMPL(OverlayManagerImpl::Container);

OverlayManagerImpl::Container::Container(WebStateList* web_state_list)
    : web_state_list_(web_state_list) {}

OverlayManagerImpl::Container::~Container() = default;

OverlayManagerImpl* OverlayManagerImpl::Container::ManagerForModality(
    OverlayModality modality) {
  auto& manager = managers_[modality];
  if (!manager) {
    manager =
        base::WrapUnique(new OverlayManagerImpl(modality, web_state_list_));
  }
  return manager.get();
}

#pragma mark - OverlayManagerImpl

OverlayManagerImpl::OverlayManagerImpl(OverlayModality modality,
                                       WebStateList* web_state_list)
    : modality_(modality), web_state_list_(web_state_list) {
  DCHECK(web_state_list_);
}

OverlayManagerImpl::~OverlayManagerImpl() = default;

void OverlayManagerImpl::AddObserver(OverlayManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void OverlayManagerImpl::RemoveObserver(OverlayManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OverlayManagerImpl::AddRequest(std::unique_ptr<OverlayRequest> request,
                                    web::WebState* web_state) {
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
  OverlayRequestQueueImpl::Container::FromWebState(web_state)
      ->QueueForModality(modality_)
      ->AddRequest(std::move(request));
}

OverlayRequestQueue* OverlayManagerImpl::GetQueueForWebState(
    web::WebState* web_state) {
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
  return OverlayRequestQueueImpl::Container::FromWebState(web_state)
      ->QueueForModality(modality_);
}
