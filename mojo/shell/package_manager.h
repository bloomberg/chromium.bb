// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PACKAGE_MANAGER_H_
#define MOJO_SHELL_PACKAGE_MANAGER_H_

#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/fetcher.h"

class GURL;

namespace mojo {
namespace shell {

class ApplicationManager;

// A class implementing this interface assists Shell::ConnectToApplication in
// fetching the applications located therein.
class PackageManager {
 public:
  PackageManager() {}
  virtual ~PackageManager() {}

  // Called once, during initialization, to tell the package manager about the
  // associated ApplicationManager.
  virtual void SetApplicationManager(ApplicationManager* manager) = 0;

  // Asks the delegate to fetch the specified url.
  // TODO(beng): figure out how not to expose Fetcher at all at this layer.
  virtual void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) = 0;

  // Determine if a content handler should handle the response received by
  // |fetcher|.
  // |url| is the url requested initially by a call to ConnectToApplication().
  // |task_runner| is a base::TaskRunner* that can be used to post callbacks.
  // |new_response| is the response that should be passed to
  // ContentHandler::StartApplication(), unchanged if the return value is false.
  // |content_handler_url| is the url of the content handler application to
  // run, unchanged if the return value is false.
  // |qualifier| is the Identity qualifier that the content handler application
  // instance should be associated with, unchanged if the return value is false.
  virtual bool HandleWithContentHandler(Fetcher* fetcher,
                                        const GURL& url,
                                        base::TaskRunner* task_runner,
                                        URLResponsePtr* new_response,
                                        GURL* content_handler_url,
                                        std::string* qualifier) = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_PACKAGE_MANAGER_H_
