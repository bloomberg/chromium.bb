// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_DELEGATE_H_

#include <string>

namespace extensions {

// Delegate interface for the push messaging invalidation handler. For each
// object that was invalidated, OnMessage() will be called back once for that
// object.
class PushMessagingInvalidationHandlerDelegate {
 public:
  virtual void OnMessage(const std::string& extension_id,
                         int subchannel,
                         const std::string& payload) = 0;

 protected:
  virtual ~PushMessagingInvalidationHandlerDelegate() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_DELEGATE_H_
