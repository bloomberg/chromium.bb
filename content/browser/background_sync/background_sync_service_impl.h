// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_H_

#include <stdint.h>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/common/background_sync_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class BackgroundSyncContextImpl;

class CONTENT_EXPORT BackgroundSyncServiceImpl
    : public NON_EXPORTED_BASE(BackgroundSyncService) {
 public:
  BackgroundSyncServiceImpl(
      BackgroundSyncContextImpl* background_sync_context,
      mojo::InterfaceRequest<BackgroundSyncService> request);

  ~BackgroundSyncServiceImpl() override;

 private:
  friend class BackgroundSyncServiceImplTest;

  // BackgroundSyncService methods:
  void Register(content::SyncRegistrationPtr options,
                int64_t sw_registration_id,
                bool requested_from_service_worker,
                const RegisterCallback& callback) override;
  void Unregister(BackgroundSyncRegistrationHandle::HandleId handle_id,
                  int64_t sw_registration_id,
                  const UnregisterCallback& callback) override;
  void GetRegistration(const mojo::String& tag,
                       int64_t sw_registration_id,
                       const GetRegistrationCallback& callback) override;
  void GetRegistrations(int64_t sw_registration_id,
                        const GetRegistrationsCallback& callback) override;
  void DuplicateRegistrationHandle(
      BackgroundSyncRegistrationHandle::HandleId handle_id,
      const DuplicateRegistrationHandleCallback& callback) override;
  void ReleaseRegistration(
      BackgroundSyncRegistrationHandle::HandleId handle_id) override;
  void NotifyWhenFinished(BackgroundSyncRegistrationHandle::HandleId handle_id,
                          const NotifyWhenFinishedCallback& callback) override;

  void OnRegisterResult(const RegisterCallback& callback,
                        BackgroundSyncStatus status,
                        scoped_ptr<BackgroundSyncRegistrationHandle> result);
  void OnUnregisterResult(const UnregisterCallback& callback,
                          BackgroundSyncStatus status);
  void OnGetRegistrationsResult(
      const GetRegistrationsCallback& callback,
      BackgroundSyncStatus status,
      scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>> result);
  void OnNotifyWhenFinishedResult(const NotifyWhenFinishedCallback& callback,
                                  BackgroundSyncStatus status,
                                  BackgroundSyncState sync_state);

  // Called when an error is detected on binding_.
  void OnConnectionError();

  // background_sync_context_ owns this.
  BackgroundSyncContextImpl* background_sync_context_;

  mojo::Binding<BackgroundSyncService> binding_;

  // The registrations that the client might reference.
  IDMap<BackgroundSyncRegistrationHandle,
        IDMapOwnPointer,
        BackgroundSyncRegistrationHandle::HandleId> active_handles_;

  base::WeakPtrFactory<BackgroundSyncServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_H_
