// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSER_STATE_H_
#define IOS_WEB_PUBLIC_BROWSER_STATE_H_

#include "base/supports_user_data.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

namespace web {

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class BrowserState : public base::SupportsUserData {
 public:
  ~BrowserState() override;

  // Returns whether this BrowserState is incognito. Default is false.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the path where the BrowserState data is stored.
  // Unlike Profile::GetPath(), incognito BrowserState do not share their path
  // with their original BrowserState.
  virtual base::FilePath GetStatePath() const = 0;

  // Returns the request context information associated with this
  // BrowserState.
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;

  // Safely cast a base::SupportsUserData to a BrowserState. Returns nullptr
  // if |supports_user_data| is not a BrowserState.
  static BrowserState* FromSupportsUserData(
      base::SupportsUserData* supports_user_data);

 protected:
  BrowserState();
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSER_STATE_H_
