// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_MAPPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_MAPPER_H_

#include <string>

namespace extensions {

// Interface for mapping extension IDs to object IDs.
class PushMessagingInvalidationMapper {
 public:
  virtual ~PushMessagingInvalidationMapper() {}

  // Register/unregister the object IDs associated with |extension_id|.
  virtual void RegisterExtension(const std::string& extension_id) = 0;
  virtual void UnregisterExtension(const std::string& extension_id) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_MAPPER_H_
