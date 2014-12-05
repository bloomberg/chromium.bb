// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_WEB_STATE_H_
#define IOS_PUBLIC_PROVIDER_WEB_WEB_STATE_H_

#include "base/supports_user_data.h"

namespace ios {

// Core interface for interaction with the web.
class WebState : public base::SupportsUserData {
 public:
  ~WebState() override {}

 protected:
  WebState() {}
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_WEB_WEB_STATE_H_
