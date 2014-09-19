// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_

#include "base/android/jni_weak_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"

class GURL;

namespace content {
class ResourceThrottle;
class WebContents;
}

namespace net {
class URLRequest;
}

namespace navigation_interception {

class NavigationParams;

// Native side of the InterceptNavigationDelegate Java interface.
// This is used to create a InterceptNavigationResourceThrottle that calls the
// Java interface method to determine whether a navigation should be ignored or
// not.
// To us this class:
// 1) the Java-side interface implementation must be associated (via the
//    Associate method) with a WebContents for which URLRequests are to be
//    intercepted,
// 2) the ResourceThrottle obtained via CreateThrottleFor must be associated
//    with the URLRequests in the ResourceDispatcherHostDelegate
//    implementation.
class InterceptNavigationDelegate : public base::SupportsUserData::Data {
 public:
  InterceptNavigationDelegate(JNIEnv* env, jobject jdelegate);
  virtual ~InterceptNavigationDelegate();

  // Associates the InterceptNavigationDelegate with a WebContents using the
  // SupportsUserData mechanism.
  // As implied by the use of scoped_ptr, the WebContents will assume ownership
  // of |delegate|.
  static void Associate(content::WebContents* web_contents,
                        scoped_ptr<InterceptNavigationDelegate> delegate);
  // Gets the InterceptNavigationDelegate associated with the WebContents,
  // can be null.
  static InterceptNavigationDelegate* Get(content::WebContents* web_contents);

  // Creates a InterceptNavigationResourceThrottle that will direct all
  // callbacks to the InterceptNavigationDelegate.
  static content::ResourceThrottle* CreateThrottleFor(
      net::URLRequest* request);

  virtual bool ShouldIgnoreNavigation(
      const NavigationParams& navigation_params);
 private:
  JavaObjectWeakGlobalRef weak_jdelegate_;
};

bool RegisterInterceptNavigationDelegate(JNIEnv* env);

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
