// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_observer_registrar.h"

#include "content/browser/tab_contents/tab_contents.h"

TabContentsObserverRegistrar::TabContentsObserverRegistrar(
    TabContentsObserver* observer)
    : observer_(observer), tab_(NULL) {
}

TabContentsObserverRegistrar::~TabContentsObserverRegistrar() {
  if (tab_) {
    tab_->RemoveObserver(observer_);
    tab_->RemoveObserver(this);
  }
}

void TabContentsObserverRegistrar::Observe(TabContents* tab) {
  observer_->SetTabContents(tab);
  SetTabContents(tab);
  if (tab_) {
    tab_->RemoveObserver(observer_);
    tab_->RemoveObserver(this);
  }
  tab_ = tab;
  if (tab_) {
    tab_->AddObserver(observer_);
    tab_->AddObserver(this);
  }
}

void TabContentsObserverRegistrar::TabContentsDestroyed(TabContents* tab) {
  Observe(NULL);
}
