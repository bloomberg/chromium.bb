// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/active_state_manager_impl.h"

#include "base/logging.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"

namespace web {

namespace {
// The number of ActiveStateManagers that are currently in active state.
// At most one ActiveStateManager can be active at any given time.
int g_active_state_manager_active_count = 0;
}  // namespace

ActiveStateManagerImpl::ActiveStateManagerImpl(BrowserState* browser_state)
    : browser_state_(browser_state), active_(false) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(browser_state_);
}

ActiveStateManagerImpl::~ActiveStateManagerImpl() {
  FOR_EACH_OBSERVER(Observer, observer_list_, WillBeDestroyed());
  DCHECK(!IsActive());
}

void ActiveStateManagerImpl::SetActive(bool active) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  if (active == active_) {
    return;
  }
  if (active) {
    ++g_active_state_manager_active_count;
  } else {
    --g_active_state_manager_active_count;
  }
  DCHECK_GE(1, g_active_state_manager_active_count);
  active_ = active;

  if (active) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnActive());
  } else {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnInactive());
  }
}

bool ActiveStateManagerImpl::IsActive() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  return active_;
}

void ActiveStateManagerImpl::AddObserver(ActiveStateManager::Observer* obs) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  observer_list_.AddObserver(obs);
}

void ActiveStateManagerImpl::RemoveObserver(ActiveStateManager::Observer* obs) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  observer_list_.RemoveObserver(obs);
}

}  // namespace web
