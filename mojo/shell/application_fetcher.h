// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_FETCHER_H_
#define MOJO_SHELL_APPLICATION_FETCHER_H_

#include "mojo/shell/fetcher.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

class GURL;

namespace mojo {
namespace shell {

class ApplicationManager;

// A class implementing this interface assists Shell::ConnectToApplication in
// resolving URLs and fetching the applications located therein.
class ApplicationFetcher {
 public:
  virtual ~ApplicationFetcher() {}

  // Called once, during initialization, to tell the fetcher about the
  // associated ApplicationManager.
  virtual void SetApplicationManager(ApplicationManager* manager) = 0;

  // Resolves |url| to its canonical form. e.g. for mojo: urls, returns a file:
  // url with a path ending in .mojo.
  virtual GURL ResolveURL(const GURL& url) = 0;

  // Asks the delegate to fetch the specified url. |url| must be unresolved,
  // i.e. ResolveURL() above must not have been called on it.
  // TODO(beng): figure out how not to expose Fetcher at all at this layer.
  virtual void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_FETCHER_H_
