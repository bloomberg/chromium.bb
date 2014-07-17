// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_FAKE_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_FAKE_INVALIDATOR_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "components/invalidation/invalidator.h"
#include "components/invalidation/invalidator_registrar.h"
#include "sync/internal_api/public/base/invalidation_util.h"

namespace syncer {

class FakeInvalidator : public Invalidator {
 public:
  FakeInvalidator();
  virtual ~FakeInvalidator();

  bool IsHandlerRegistered(InvalidationHandler* handler) const;
  ObjectIdSet GetRegisteredIds(InvalidationHandler* handler) const;
  const std::string& GetUniqueId() const;
  const std::string& GetCredentialsEmail() const;
  const std::string& GetCredentialsToken() const;

  void EmitOnInvalidatorStateChange(InvalidatorState state);
  void EmitOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map);

  virtual void RegisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) const
    OVERRIDE;

 private:
  InvalidatorRegistrar registrar_;
  std::string state_;
  std::string email_;
  std::string token_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_FAKE_INVALIDATOR_H_
