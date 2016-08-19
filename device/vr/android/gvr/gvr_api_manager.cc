// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_api_manager.h"

namespace device {

namespace {
GvrApiManager* g_gvr_api_manager = nullptr;
}

GvrApiManager* GvrApiManager::GetInstance() {
  if (!g_gvr_api_manager)
    g_gvr_api_manager = new GvrApiManager();
  return g_gvr_api_manager;
}

GvrApiManager::GvrApiManager() {}

GvrApiManager::~GvrApiManager() {}

void GvrApiManager::AddClient(GvrApiManagerClient* client) {
  clients_.push_back(client);

  if (gvr_api_)
    client->OnGvrApiInitialized(gvr_api_.get());
}

void GvrApiManager::RemoveClient(GvrApiManagerClient* client) {
  clients_.erase(std::remove(clients_.begin(), clients_.end(), client),
                 clients_.end());
}

void GvrApiManager::Initialize(gvr_context* context) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(context);

  for (const auto& client : clients_)
    client->OnGvrApiInitialized(gvr_api_.get());
}

void GvrApiManager::Shutdown() {
  if (!gvr_api_)
    return;

  gvr_api_.reset(nullptr);

  for (const auto& client : clients_)
    client->OnGvrApiShutdown();
}

gvr::GvrApi* GvrApiManager::gvr_api() {
  return gvr_api_.get();
}

}  // namespace device
