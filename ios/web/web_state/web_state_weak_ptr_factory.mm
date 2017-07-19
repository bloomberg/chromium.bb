// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_state_weak_ptr_factory.h"

#import "ios/web/public/web_state/web_state.h"

namespace web {

// static
base::WeakPtr<WebState> WebStateWeakPtrFactory::CreateWeakPtr(
    WebState* web_state) {
  if (!web_state)
    return base::WeakPtr<WebState>();
  return web_state->AsWeakPtr();
}

}  // namespace web
