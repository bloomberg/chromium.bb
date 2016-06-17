// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_contents_impl.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/public/blimp_contents_observer.h"

namespace blimp {
namespace client {

BlimpContentsImpl::BlimpContentsImpl() : navigation_controller_(this) {}

BlimpContentsImpl::~BlimpContentsImpl() {}

BlimpNavigationControllerImpl& BlimpContentsImpl::GetNavigationController() {
  return navigation_controller_;
}

void BlimpContentsImpl::AddObserver(BlimpContentsObserver* observer) {
  observers_.AddObserver(observer);
}

void BlimpContentsImpl::RemoveObserver(BlimpContentsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BlimpContentsImpl::NotifyURLLoaded(const GURL& url) {
  FOR_EACH_OBSERVER(BlimpContentsObserver, observers_, OnURLUpdated(url));
}

}  // namespace client
}  // namespace blimp
