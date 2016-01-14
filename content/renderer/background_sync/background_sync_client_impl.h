// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BACKGROUND_SYNC_BACKGROUND_SYNC_CLIENT_IMPL_H_
#define CONTENT_RENDERER_BACKGROUND_SYNC_BACKGROUND_SYNC_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncClientImpl
    : public NON_EXPORTED_BASE(BackgroundSyncServiceClient) {
 public:
  static void Create(
      mojo::InterfaceRequest<BackgroundSyncServiceClient> request);

  ~BackgroundSyncClientImpl() override;

 private:
  using SyncCallback = mojo::Callback<void(ServiceWorkerEventStatus)>;
  explicit BackgroundSyncClientImpl(
      mojo::InterfaceRequest<BackgroundSyncServiceClient> request);

  // BackgroundSyncServiceClient methods:
  void Sync(int64_t handle_id,
            content::BackgroundSyncEventLastChance last_chance,
            const SyncCallback& callback) override;
  void SyncDidGetRegistration(
      int64_t callback_id,
      content::BackgroundSyncEventLastChance last_chance,
      BackgroundSyncError error,
      SyncRegistrationPtr registration);

  mojo::StrongBinding<BackgroundSyncServiceClient> binding_;

  int64_t callback_seq_num_;
  std::map<int64_t, SyncCallback> sync_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BACKGROUND_SYNC_BACKGROUND_SYNC_CLIENT_IMPL_H_
