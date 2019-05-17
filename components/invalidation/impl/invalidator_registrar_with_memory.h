// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_WITH_MEMORY_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_WITH_MEMORY_H_

#include <map>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/impl/invalidator_registrar.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;
class PrefService;

namespace syncer {


// A helper class for implementations of the Invalidator interface.  It helps
// keep track of registered handlers and which object ID registrations are
// associated with which handlers, so implementors can just reuse the logic
// here to dispatch invalidations and other interesting notifications.
class INVALIDATION_EXPORT InvalidatorRegistrarWithMemory
    : public InvalidatorRegistrar {
 public:
  InvalidatorRegistrarWithMemory(PrefService* local_state,
                                 const std::string& sender_id,
                                 bool migrate_old_prefs);

  // It is an error to have registered handlers on destruction.
  ~InvalidatorRegistrarWithMemory();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Updates the set of topics associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  A topic must be registered
  // for at most one handler. If topic is already registered function returns
  // false.
  bool UpdateRegisteredTopics(InvalidationHandler* handler,
                              const Topics& topics) override WARN_UNUSED_RESULT;

  // void UnregisterHandler(InvalidationHandler* handler) override;
  // void RegisterHandler(InvalidationHandler* handler) override;

  // Returns the set of all IDs that are registered to some handler (even
  // handlers that have been unregistered).
  Topics GetAllRegisteredIds() const override;

  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> callback)
      const;

 private:
  using HandlerNameTopicsMap = std::map<std::string, Topics>;

  // Generate a Dictionary with all the debugging information.
  base::DictionaryValue CollectDebugData() const;

  std::unordered_map<std::string, InvalidationHandler*>
      handler_name_to_handler_;
  HandlerNameTopicsMap handler_name_to_topics_map_;
  PrefService* local_state_;
  const std::string sender_id_;

  DISALLOW_COPY_AND_ASSIGN(InvalidatorRegistrarWithMemory);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_WITH_MEMORY_H_
