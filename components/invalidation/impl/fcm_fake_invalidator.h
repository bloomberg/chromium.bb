// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_FAKE_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_FAKE_INVALIDATOR_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/invalidator_registrar.h"
#include "components/invalidation/public/invalidation_util.h"

namespace syncer {

class FCMFakeInvalidator : public Invalidator {
 public:
  FCMFakeInvalidator();
  ~FCMFakeInvalidator() override;

  bool IsHandlerRegistered(InvalidationHandler* handler) const;
  ObjectIdSet GetRegisteredIds(InvalidationHandler* handler) const;
  const std::string& GetUniqueId() const;
  const std::string& GetCredentialsEmail() const;
  const std::string& GetCredentialsToken() const;

  void EmitOnInvalidatorStateChange(InvalidatorState state);
  void EmitOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map);

  void RegisterHandler(InvalidationHandler* handler) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const ObjectIdSet& ids) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const TopicSet& topics) override;
  void UnregisterHandler(InvalidationHandler* handler) override;
  InvalidatorState GetInvalidatorState() const override;
  void UpdateCredentials(const std::string& email,
                         const std::string& token) override;
  void RequestDetailedStatus(base::Callback<void(const base::DictionaryValue&)>
                                 callback) const override;

 private:
  InvalidatorRegistrar registrar_;
  std::string state_;
  std::string email_;
  std::string token_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_FAKE_INVALIDATOR_H_
