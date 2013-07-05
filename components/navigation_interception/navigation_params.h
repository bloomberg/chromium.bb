// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_H_

#include "content/public/common/page_transition_types.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace navigation_interception {

class NavigationParams {
 public:
  NavigationParams(const GURL& url,
                   const content::Referrer& referrer,
                   bool has_user_gesture,
                   bool is_post,
                   content::PageTransition page_transition_type,
                   bool is_redirect);
  NavigationParams(const NavigationParams& other);
  void operator=(const NavigationParams& rhs);

  const GURL& url() const { return url_; }
  GURL& url() { return url_; }
  const content::Referrer& referrer() const { return referrer_; }
  bool has_user_gesture() const { return has_user_gesture_; }
  bool is_post() const { return is_post_; }
  content::PageTransition transition_type() const { return transition_type_; }
  bool is_redirect() const { return is_redirect_; }

 private:
  void Assign(const NavigationParams& other);

  GURL url_;
  content::Referrer referrer_;
  bool has_user_gesture_;
  bool is_post_;
  content::PageTransition transition_type_;
  bool is_redirect_;
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_H_
