// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DEFERRED_START_RENDER_HOST_OBSERVER_H_
#define EXTENSIONS_BROWSER_DEFERRED_START_RENDER_HOST_OBSERVER_H_

namespace extensions {
class DeferredStartRenderHost;

// Observer of DeferredStartRenderHost lifetime.
//
// TODO(devlin): Move OnDeferredStartRenderHostDidStopFirstLoad() into
// ExtensionHostObserver, and remove this class.
class DeferredStartRenderHostObserver {
 public:
  virtual ~DeferredStartRenderHostObserver() {}

  // Called when a DeferredStartRenderHost stopped loading.
  virtual void OnDeferredStartRenderHostDidStopFirstLoad(
      const DeferredStartRenderHost* host) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DEFERRED_START_RENDER_HOST_OBSERVER_H_
