// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_mapper.h"
#include "sync/notifier/invalidation_handler.h"

class InvalidationFrontend;

namespace extensions {

class PushMessagingInvalidationHandlerDelegate;

// This class maps extension IDs to the corresponding object IDs, manages
// invalidation registrations, and dispatches callbacks for any invalidations
// received.
class PushMessagingInvalidationHandler : public PushMessagingInvalidationMapper,
                                         public syncer::InvalidationHandler {
 public:
  // |service| and |delegate| must remain valid for the lifetime of this object.
  // |extension_ids| is the set of extension IDs for which push messaging is
  // enabled.
  PushMessagingInvalidationHandler(
      InvalidationFrontend* service,
      PushMessagingInvalidationHandlerDelegate* delegate,
      const std::set<std::string>& extension_ids);
  virtual ~PushMessagingInvalidationHandler();

  // PushMessagingInvalidationMapper implementation.
  virtual void RegisterExtension(const std::string& extension_id) OVERRIDE;
  virtual void UnregisterExtension(const std::string& extension_id) OVERRIDE;

  // InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdStateMap& id_state_map,
      syncer::IncomingInvalidationSource source) OVERRIDE;

  const std::set<std::string>& GetRegisteredExtensionsForTest() const {
    return registered_extensions_;
  }

 private:
  void UpdateRegistrations();

  base::ThreadChecker thread_checker_;
  InvalidationFrontend* const service_;
  std::set<std::string> registered_extensions_;
  PushMessagingInvalidationHandlerDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingInvalidationHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_H_
