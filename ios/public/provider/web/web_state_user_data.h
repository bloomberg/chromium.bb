// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_WEB_STATE_USER_DATA_H_
#define IOS_PUBLIC_PROVIDER_WEB_WEB_STATE_USER_DATA_H_

#include "ios/public/consumer/base/supports_user_data.h"
#include "ios/public/provider/web/web_state.h"

namespace ios {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebState. For example:
//
// --- in foo.h ---
// class Foo : public ios::WebStateUserData<Foo> {
//  public:
//   virtual ~Foo();
//   // ... more public stuff here ...
//  private:
//   explicit Foo(ios::WebState* web_state);
//   friend class ios::WebStateUserData<Foo>;
//   // ... more private stuff here ...
// }
// --- in foo.cc ---
// DEFINE_WEB_CONTENTS_USER_DATA_KEY(Foo);
//
template <typename T>
class WebStateUserData : public ios::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebState.
  // If an instance is already attached, does nothing.
  static void CreateForWebState(WebState* web_state) {
    if (!FromWebState(web_state))
      web_state->SetUserData(UserDataKey(), new T(web_state));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebState (via CreateForWebState above) and returns it. If no instance
  // of the type was attached, returns NULL.
  static T* FromWebState(WebState* web_state) {
    return static_cast<T*>(web_state->GetUserData(UserDataKey()));
  }
  static const T* FromWebState(const WebState* web_state) {
    return static_cast<const T*>(web_state->GetUserData(UserDataKey()));
  }

 protected:
  static inline void* UserDataKey() {
    return &kLocatorKey;
  }

 private:
  // The user data key.
  static int kLocatorKey;
};

// The macro to define the locator key. This key should be defined in the .cc
// file of the derived class.
//
// The "= 0" is surprising, but is required to effect a definition rather than
// a declaration. Without it, this would be merely a declaration of a template
// specialization. (C++98: 14.7.3.15; C++11: 14.7.3.13)
//
#define DEFINE_WEB_STATE_USER_DATA_KEY(TYPE) \
template<>                                      \
int ios::WebStateUserData<TYPE>::kLocatorKey = 0

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_WEB_WEB_STATE_USER_DATA_H_
