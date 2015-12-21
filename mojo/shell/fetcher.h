// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FETCHER_H_
#define MOJO_SHELL_FETCHER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

class GURL;

namespace base {
class FilePath;
class TaskRunner;
}

namespace mojo {
namespace shell {

// Fetcher abstracts getting an application by either file or http[s] URL.
//
// Although it is possible to use the Network implementation for http[s] URLs
// (because the underlying net library knows how to handle them), it is
// extremely slow because network responses must be copied to disk in order to
// get a file handle we can use with dlopen.
//
// Until this is solved, we use two different implementations so that
// performance isn't completely absymal.
class Fetcher {
 public:
  // The param will be null in the case where the content could not be fetched.
  // Reasons include:
  // - network error
  typedef base::Callback<void(scoped_ptr<Fetcher>)> FetchCallback;

  Fetcher(const FetchCallback& fetch_callback);
  virtual ~Fetcher();

  // Returns the original URL that was fetched.
  virtual const GURL& GetURL() const = 0;

  // If the fetch resulted in a redirect, this returns the final URL after all
  // redirects. Otherwise, it returns an empty URL.
  virtual GURL GetRedirectURL() const = 0;

  // If the fetch resulted in a redirect, this returns the referer URL to use
  // with the redirect.
  virtual GURL GetRedirectReferer() const = 0;

  virtual URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                                       uint32_t skip) = 0;

  virtual void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) = 0;

  virtual std::string MimeType() = 0;

  virtual bool HasMojoMagic() = 0;

  virtual bool PeekFirstLine(std::string* line) = 0;

  bool PeekContentHandler(std::string* mojo_shebang,
                          GURL* mojo_content_handler_url);

 protected:
  static const char kMojoMagic[];
  static const size_t kMaxShebangLength;

  FetchCallback loader_callback_;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_FETCHER_H_
