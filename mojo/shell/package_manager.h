// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PACKAGE_MANAGER_H_
#define MOJO_SHELL_PACKAGE_MANAGER_H_

#include <stdint.h>

#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/public/interfaces/application.mojom.h"

class GURL;

namespace mojo {
namespace shell {

class ApplicationManager;
class Identity;

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
  // |source| is the identity of the application that issued the request.
  // |target_url| is the URL of that may be content that could be handled by a
  // content handler.
  // |target_filter| is a CapabilityFilter that should be applied if a content
  // handler is started to handle |target_url|.
  // |application_request| is a request for an Application implementation that
  // will be taken by ContentHandler::StartApplication if a content handler ends
  // up handling |target_url|.
  virtual uint32_t HandleWithContentHandler(
      Fetcher* fetcher,
      const Identity& source,
      const GURL& target_url,
      const CapabilityFilter& target_filter,
      InterfaceRequest<shell::mojom::Application>* application_request) = 0;

  // Returns true if a manifest for |url| exists within the PackageManager's
  // application catalog.
  virtual bool IsURLInCatalog(const std::string& url) const = 0;

  // Returns the name for the application at |url| from its manifest.
  virtual std::string GetApplicationName(const std::string& url) const = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_PACKAGE_MANAGER_H_
