// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_HANDLER_H_

#include "extensions/common/manifest_handler.h"

namespace extensions {
namespace declarative_net_request {

// Parses the kDeclarativeNetRequestKey manifest key.
class DNRManifestHandler : public ManifestHandler {
 public:
  DNRManifestHandler();
  ~DNRManifestHandler() override;

 private:
  bool Parse(Extension* extension, base::string16* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(DNRManifestHandler);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_HANDLER_H_
