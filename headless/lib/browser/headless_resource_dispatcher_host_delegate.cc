// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_resource_dispatcher_host_delegate.h"

namespace headless {

HeadlessResourceDispatcherHostDelegate::HeadlessResourceDispatcherHostDelegate(
    bool enable_resource_scheduler)
    : enable_resource_scheduler_(enable_resource_scheduler) {}

HeadlessResourceDispatcherHostDelegate::
    ~HeadlessResourceDispatcherHostDelegate() {}

bool HeadlessResourceDispatcherHostDelegate::ShouldUseResourceScheduler()
    const {
  return enable_resource_scheduler_;
}

}  // namespace headless
