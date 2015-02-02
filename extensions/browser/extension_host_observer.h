// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_OBSERVER_H_

#include <string>

namespace extensions {
class ExtensionHost;

class ExtensionHostObserver {
 public:
  virtual ~ExtensionHostObserver() {}

  // Called when an ExtensionHost is destroyed.
  virtual void OnExtensionHostDestroyed(const ExtensionHost* host) {}

  // Called when a message has been disptached to the RenderView corresponding
  // to |host|.
  virtual void OnExtensionMessageDispatched(const ExtensionHost* host,
                                            const std::string& event_name,
                                            int message_id) {}

  // Called when a previously dispatched message has been acked by the
  // RenderView for |host|.
  virtual void OnExtensionMessageAcked(const ExtensionHost* host,
                                       int message_id) {}

  // Called when the extension associated with |host| starts a new network
  // request.
  virtual void OnNetworkRequestStarted(const ExtensionHost* host,
                                       uint64 request_id) {}

  // Called when the network request with |request_id| is done.
  virtual void OnNetworkRequestDone(const ExtensionHost* host,
                                    uint64 request_id) {}
};

}  // namespace extensions

#endif /* EXTENSIONS_BROWSER_EXTENSION_HOST_OBSERVER_H_ */
