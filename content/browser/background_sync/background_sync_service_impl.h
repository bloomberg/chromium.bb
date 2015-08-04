// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/common/background_sync_service.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace content {

class BackgroundSyncContextImpl;

class CONTENT_EXPORT BackgroundSyncServiceImpl
    : public NON_EXPORTED_BASE(BackgroundSyncService) {
 public:
  static void Create(
      const scoped_refptr<BackgroundSyncContextImpl>& background_sync_context,
      mojo::InterfaceRequest<BackgroundSyncService> request);

  ~BackgroundSyncServiceImpl() override;

 private:
  friend class BackgroundSyncServiceImplTest;

  static void CreateOnIOThread(
      const scoped_refptr<BackgroundSyncContextImpl>& background_sync_context,
      mojo::InterfaceRequest<BackgroundSyncService> request);

  explicit BackgroundSyncServiceImpl(
      const scoped_refptr<BackgroundSyncContextImpl>& background_sync_context,
      mojo::InterfaceRequest<BackgroundSyncService> request);

  // BackgroundSyncService methods:
  void Register(content::SyncRegistrationPtr options,
                int64_t sw_registration_id,
                const RegisterCallback& callback) override;
  void Unregister(BackgroundSyncPeriodicity periodicity,
                  int64_t id,
                  const mojo::String& tag,
                  int64_t sw_registration_id,
                  const UnregisterCallback& callback) override;
  void GetRegistration(BackgroundSyncPeriodicity periodicity,
                       const mojo::String& tag,
                       int64_t sw_registration_id,
                       const GetRegistrationCallback& callback) override;
  void GetRegistrations(BackgroundSyncPeriodicity periodicity,
                        int64_t sw_registration_id,
                        const GetRegistrationsCallback& callback) override;
  void GetPermissionStatus(
      BackgroundSyncPeriodicity periodicity,
      int64_t sw_registration_id,
      const GetPermissionStatusCallback& callback) override;

  void OnRegisterResult(const RegisterCallback& callback,
                        BackgroundSyncStatus status,
                        const BackgroundSyncRegistration& result);
  void OnUnregisterResult(const UnregisterCallback& callback,
                          BackgroundSyncStatus status);
  void OnGetRegistrationsResult(
      const GetRegistrationsCallback& callback,
      BackgroundSyncStatus status,
      const std::vector<BackgroundSyncRegistration>& result);

  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  mojo::StrongBinding<BackgroundSyncService> binding_;

  base::WeakPtrFactory<BackgroundSyncServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_H_
