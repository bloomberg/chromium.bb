// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/navigation_params.h"

namespace navigation_interception {

NavigationParams::NavigationParams(const NavigationParams& other) {
  Assign(other);
}

NavigationParams::NavigationParams(
    const GURL& url,
    const content::Referrer& referrer,
    bool has_user_gesture,
    bool is_post,
    content::PageTransition transition_type,
    bool is_redirect)
    : url_(url),
      referrer_(referrer),
      has_user_gesture_(has_user_gesture),
      is_post_(is_post),
      transition_type_(transition_type),
      is_redirect_(is_redirect) {
}

void NavigationParams::operator=(const NavigationParams& rhs) {
  Assign(rhs);
}

void NavigationParams::Assign(const NavigationParams& other) {
  url_ = other.url();
  referrer_ = other.referrer();
  has_user_gesture_ = other.has_user_gesture();
  is_post_ = other.is_post();
  transition_type_ = other.transition_type();
  is_redirect_ = other.is_redirect();
}

}  // namespace navigation_interception

