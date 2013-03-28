// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_ANDROID_INVALIDATOR_BRIDGE_PROXY_H_
#define CHROME_BROWSER_SYNC_GLUE_ANDROID_INVALIDATOR_BRIDGE_PROXY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/notifier/invalidator.h"

namespace browser_sync {

class AndroidInvalidatorBridge;

// This class implements the Invalidator interface by wrapping (but not taking
// ownership of) an AndroidInvalidatorBridge.  This is useful because the
// SyncManager currently expects to take ownership of its invalidator, but it is
// not prepared to take ownership of the UI-thread-owned
// AndroidInvalidatorBridge.  So we use this class to wrap the
// AndroidInvalidator and allow the SyncManager to own it instead.
class AndroidInvalidatorBridgeProxy : public syncer::Invalidator {
 public:
  // Does not take ownership of |bridge|.
  explicit AndroidInvalidatorBridgeProxy(AndroidInvalidatorBridge* bridge);
  virtual ~AndroidInvalidatorBridgeProxy();

  // Invalidator implementation. Passes through all calls to the bridge.
  virtual void RegisterHandler(syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(syncer::InvalidationHandler * handler,
                                   const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const syncer::AckHandle& ack_handle) OVERRIDE;
  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  // The notification bridge that we forward to but don't own.
  AndroidInvalidatorBridge* const bridge_;

  DISALLOW_COPY_AND_ASSIGN(AndroidInvalidatorBridgeProxy);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_ANDROID_INVALIDATOR_BRIDGE_PROXY_H_
