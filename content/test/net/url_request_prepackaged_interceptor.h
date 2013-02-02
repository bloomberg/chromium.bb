// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_

#include "base/basictypes.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {

// Intercepts HTTP requests and gives pre-defined responses to specified URLs.
// The pre-defined responses are loaded from files on disk.  The interception
// occurs while the URLRequestPrepackagedInterceptor is alive.
class URLRequestPrepackagedInterceptor {
 public:
  URLRequestPrepackagedInterceptor();
  virtual ~URLRequestPrepackagedInterceptor();

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname of |url| must be "localhost" to avoid DNS lookups, and the scheme
  // must be "http".
  void SetResponse(const GURL& url, const base::FilePath& path);

  // Identical to SetResponse except that query parameters are ignored on
  // incoming URLs when comparing against |url|.
  void SetResponseIgnoreQuery(const GURL& url, const base::FilePath& path);

  // Returns how many requests have been issued that have a stored reply.
  int GetHitCount();

 private:
  class Delegate;

  // After creation, |delegate_| lives on the IO thread, and a task to delete
  // it is posted from ~URLRequestPrepackagedInterceptor().
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPrepackagedInterceptor);
};

}  // namespace content

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
