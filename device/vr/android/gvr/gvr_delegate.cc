// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace device {

GvrDelegateManager* GvrDelegateManager::GetInstance() {
  return base::Singleton<GvrDelegateManager>::get();
}

GvrDelegateManager::GvrDelegateManager() : delegate_(nullptr) {}

GvrDelegateManager::~GvrDelegateManager() {}

void GvrDelegateManager::AddClient(GvrDelegateClient* client) {
  clients_.push_back(client);

  if (delegate_)
    client->OnDelegateInitialized(delegate_);
}

void GvrDelegateManager::RemoveClient(GvrDelegateClient* client) {
  clients_.erase(std::remove(clients_.begin(), clients_.end(), client),
                 clients_.end());
}

void GvrDelegateManager::Initialize(GvrDelegate* delegate) {
  // Don't initialize the delegate manager twice.
  DCHECK(!delegate_);

  delegate_ = delegate;

  for (const auto& client : clients_)
    client->OnDelegateInitialized(delegate_);
}

void GvrDelegateManager::Shutdown() {
  if (!delegate_)
    return;

  delegate_ = nullptr;

  for (const auto& client : clients_)
    client->OnDelegateShutdown();
}

}  // namespace device
