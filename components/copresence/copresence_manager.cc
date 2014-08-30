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

  // This callback will be canceled on manager's destruction, hence unretained
  // is safe to use here.
  manager->init_callback_.Reset(
      base::Bind(&CopresenceManagerImpl::InitStepComplete,
                 base::Unretained(manager),
                 "Whispernet proxy initialization"));

  manager->pending_init_operations_++;
  DCHECK(delegate->GetWhispernetClient());
  delegate->GetWhispernetClient()->Initialize(
      manager->init_callback_.callback());

  return make_scoped_ptr<CopresenceManager>(manager);
}

}  // namespace copresence

