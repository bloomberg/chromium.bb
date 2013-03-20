// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_MAPPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_MAPPER_H_

#include <string>

namespace extensions {

// Interface for mapping extension IDs to object IDs.
// TODO(dcheng): This interface is somewhat misnamed... think of a better name.
class PushMessagingInvalidationMapper {
 public:
  virtual ~PushMessagingInvalidationMapper() {}

  // Informs the mapper that |extension_id| was just installed. The mapper uses
  // this information to suppress the initial invalidation on each subchannel
  // for that extension to avoid spurious push messages at install time.
  virtual void SuppressInitialInvalidationsForExtension(
      const std::string& extension_id) = 0;

  // Register/unregister the object IDs associated with |extension_id|.
  virtual void RegisterExtension(const std::string& extension_id) = 0;
  virtual void UnregisterExtension(const std::string& extension_id) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_MAPPER_H_
