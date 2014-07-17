// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_mapper.h"
#include "components/invalidation/invalidation_handler.h"
#include "sync/internal_api/public/base/invalidation_util.h"

namespace invalidation {
class InvalidationService;
}

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
      invalidation::InvalidationService* service,
      PushMessagingInvalidationHandlerDelegate* delegate);
  virtual ~PushMessagingInvalidationHandler();

  // PushMessagingInvalidationMapper implementation.
  virtual void SuppressInitialInvalidationsForExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void RegisterExtension(const std::string& extension_id) OVERRIDE;
  virtual void UnregisterExtension(const std::string& extension_id) OVERRIDE;

  // InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual std::string GetOwnerName() const OVERRIDE;

  const std::set<std::string>& GetRegisteredExtensionsForTest() const {
    return registered_extensions_;
  }

 private:
  void UpdateRegistrations();

  base::ThreadChecker thread_checker_;
  invalidation::InvalidationService* const service_;
  std::set<std::string> registered_extensions_;
  syncer::ObjectIdSet suppressed_ids_;
  PushMessagingInvalidationHandlerDelegate* const delegate_;
  std::map<invalidation::ObjectId,
           int64,
           syncer::ObjectIdLessThan> max_object_version_map_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingInvalidationHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_INVALIDATION_HANDLER_H_
