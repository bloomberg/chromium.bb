// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/active_state_manager_impl.h"

#include "base/logging.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ActiveStateManagerImpl::ActiveStateManagerImpl(BrowserState* browser_state)
    : browser_state_(browser_state), active_(false) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(browser_state_);
}

ActiveStateManagerImpl::~ActiveStateManagerImpl() {
  for (auto& observer : observer_list_)
    observer.WillBeDestroyed();
  DCHECK(!IsActive());
}

void ActiveStateManagerImpl::SetActive(bool active) {
  DCHECK_CURRENTLY_ON(WebThread::UI);

  if (active == active_) {
    return;
  }
  active_ = active;

  if (active) {
    for (auto& observer : observer_list_)
      observer.OnActive();
  } else {
    for (auto& observer : observer_list_)
      observer.OnInactive();
  }
}

bool ActiveStateManagerImpl::IsActive() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  return active_;
}

void ActiveStateManagerImpl::AddObserver(ActiveStateManager::Observer* obs) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  observer_list_.AddObserver(obs);
}

void ActiveStateManagerImpl::RemoveObserver(ActiveStateManager::Observer* obs) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  observer_list_.RemoveObserver(obs);
}

}  // namespace web
