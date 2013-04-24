// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_

#include <string>

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
  // Registers an interceptor for urls using |scheme| and |hostname|. Urls
  // passed to "SetResponse" are required to use |scheme| and |hostname|.
  URLRequestPrepackagedInterceptor(const std::string& scheme,
                                   const std::string& hostname);
  virtual ~URLRequestPrepackagedInterceptor();

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname and scheme of |url| must match the corresponding parameters
  // passed as constructor arguments.
  void SetResponse(const GURL& url, const base::FilePath& path);

  // Identical to SetResponse except that query parameters are ignored on
  // incoming URLs when comparing against |url|.
  void SetResponseIgnoreQuery(const GURL& url, const base::FilePath& path);

  // Returns how many requests have been issued that have a stored reply.
  int GetHitCount();

 private:
  class Delegate;

  const std::string scheme_;
  const std::string hostname_;

  // After creation, |delegate_| lives on the IO thread, and a task to delete
  // it is posted from ~URLRequestPrepackagedInterceptor().
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPrepackagedInterceptor);
};

// Specialization of URLRequestPrepackagedInterceptor where scheme is "http" and
// hostname is "localhost".
class URLLocalHostRequestPrepackagedInterceptor
    : public URLRequestPrepackagedInterceptor {
 public:
  URLLocalHostRequestPrepackagedInterceptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(URLLocalHostRequestPrepackagedInterceptor);
};

}  // namespace content

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
