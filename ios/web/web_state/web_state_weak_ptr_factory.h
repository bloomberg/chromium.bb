// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_WEAK_PTR_FACTORY_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_WEAK_PTR_FACTORY_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace web {

class WebState;

// Usage of this factory is disallowed for new code. It is there to support
// migrating src/chrome code that used GetWebContentsByID, and will eventually
// go away (cleanup will happen as part of http://crbug.com/556736).
class WebStateWeakPtrFactory {
 public:
  // Returns a WeakPtr<> to |web_state| if possible. May returns a null
  // WeakPtr<> during testing.
  static base::WeakPtr<WebState> CreateWeakPtr(WebState* web_state);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebStateWeakPtrFactory);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_WEAK_PTR_FACTORY_H_
