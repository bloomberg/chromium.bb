// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_QUEUE_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_QUEUE_H_

namespace extensions {

class ExtensionHost;

// An interface for a queue of ExtensionHosts waiting for initialization.
// This is used to implement different throttling strategies.
class ExtensionHostQueue {
 public:
  virtual ~ExtensionHostQueue() {}

  // Adds a host to the queue for RenderView creation.
  virtual void Add(ExtensionHost* host) = 0;

  // Removes a host from the queue (for example, it may be deleted before
  // having a chance to start).
  virtual void Remove(ExtensionHost* host) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_QUEUE_H_
