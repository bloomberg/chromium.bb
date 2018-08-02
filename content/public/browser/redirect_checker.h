// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_REDIRECT_CHECKER_H_
#define CONTENT_PUBLIC_BROWSER_REDIRECT_CHECKER_H_

#include "base/memory/ref_counted.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace content {

// Checks if a redirect should be allowed. Useful to pass around state related
// to whitelisting certain redirects.
class RedirectChecker : public base::RefCountedThreadSafe<RedirectChecker> {
 public:
  // Whether a redirect specified by |redirect_info| should be allowed.
  virtual bool ShouldAllowRedirect(int32_t request_id,
                                   const net::RedirectInfo& redirect_info) = 0;

 protected:
  virtual ~RedirectChecker() {}

 private:
  friend base::RefCountedThreadSafe<RedirectChecker>;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_REDIRECT_CHECKER_H_
