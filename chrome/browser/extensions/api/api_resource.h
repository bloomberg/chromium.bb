// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

// An ApiResource represents something that an extension API manages, such as a
// socket or a serial-port connection. Typically, an ApiResourceManager will
// control the lifetime of all ApiResources of a specific derived type.
class ApiResource {
 public:
  virtual ~ApiResource();

  const std::string& owner_extension_id() const {
    return owner_extension_id_;
  }

 protected:
  explicit ApiResource(const std::string& owner_extension_id);

 private:
  // The extension that owns this resource.
  const std::string& owner_extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ApiResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_H_
