// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/public/copresence_manager.h"

#include "base/bind.h"
#include "components/copresence/copresence_manager_impl.h"
#include "components/copresence/rpc/rpc_handler.h"

namespace copresence {

// static
scoped_ptr<CopresenceManager> CopresenceManager::Create(
    CopresenceDelegate* delegate) {
  CopresenceManagerImpl* manager = new CopresenceManagerImpl(delegate);

  manager->pending_init_operations_++;
  manager->rpc_handler_.reset(new RpcHandler(delegate));
  manager->rpc_handler_->Initialize(
      base::Bind(&CopresenceManagerImpl::InitStepComplete,
                 // This is safe because the manager owns the RpcHandler.
                 base::Unretained(manager),
                 "Copresence device registration"));

  manager->pending_init_operations_++;
  DCHECK(delegate->GetWhispernetClient());
  delegate->GetWhispernetClient()->Initialize(
      base::Bind(&CopresenceManagerImpl::InitStepComplete,
                 // We cannot cancel WhispernetClient initialization.
                 // TODO(ckehoe): Get rid of this.
                 manager->AsWeakPtr(),
                 "Whispernet proxy initialization"));

  return make_scoped_ptr<CopresenceManager>(manager);
}

}  // namespace copresence

