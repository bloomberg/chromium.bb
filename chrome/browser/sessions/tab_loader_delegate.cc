// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader_delegate.h"

#if !defined(OS_CHROMEOS)

namespace {

// The timeout time after which the next tab gets loaded if the previous tab did
// not finish loading yet.
static const int kInitialDelayTimerMS = 100;

class TabLoaderDelegateImpl : public TabLoaderDelegate {
 public:
  TabLoaderDelegateImpl() {}
  ~TabLoaderDelegateImpl() override {}

  // TabLoaderDelegate:
  base::TimeDelta GetTimeoutBeforeLoadingNextTab() const override {
    return base::TimeDelta::FromMilliseconds(kInitialDelayTimerMS);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabLoaderDelegateImpl);
};
}  // namespace

// static
scoped_ptr<TabLoaderDelegate> TabLoaderDelegate::Create(
    TabLoaderCallback* callback) {
  return scoped_ptr<TabLoaderDelegate>(new TabLoaderDelegateImpl());
}

#endif  // !defined(OS_CHROMEOS)
