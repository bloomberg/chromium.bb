// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl.h"

#include <utility>

#include "background_sync_registration_handle.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// TODO(iclelland): Move these converters to mojo::TypeConverter template
// specializations.

BackgroundSyncRegistrationOptions ToBackgroundSyncRegistrationOptions(
    const SyncRegistrationPtr& in) {
  BackgroundSyncRegistrationOptions out;

  out.tag = in->tag;
  out.network_state = static_cast<SyncNetworkState>(in->network_state);
  return out;
}

SyncRegistrationPtr ToMojoRegistration(
    const BackgroundSyncRegistrationHandle& in) {
  SyncRegistrationPtr out(content::SyncRegistration::New());
  out->handle_id = in.handle_id();
  out->tag = in.options()->tag;
  out->network_state = static_cast<content::BackgroundSyncNetworkState>(
      in.options()->network_state);
  return out;
}

}  // namespace

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, manager_name) \
  static_assert(static_cast<int>(content::mojo_name) ==       \
                    static_cast<int>(content::manager_name),  \
                "mojo and manager enums must match")

// TODO(iclelland): Move these tests somewhere else
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncError::NONE,
                             BACKGROUND_SYNC_STATUS_OK);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncError::STORAGE,
                             BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncError::NOT_FOUND,
                             BACKGROUND_SYNC_STATUS_NOT_FOUND);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncError::NO_SERVICE_WORKER,
                             BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncError::NOT_ALLOWED,
                             BACKGROUND_SYNC_STATUS_NOT_ALLOWED);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncError::MAX,
                             BACKGROUND_SYNC_STATUS_NOT_ALLOWED);

COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncNetworkState::ANY,
                             SyncNetworkState::NETWORK_STATE_ANY);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncNetworkState::AVOID_CELLULAR,
                             SyncNetworkState::NETWORK_STATE_AVOID_CELLULAR);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncNetworkState::ONLINE,
                             SyncNetworkState::NETWORK_STATE_ONLINE);
COMPILE_ASSERT_MATCHING_ENUM(BackgroundSyncNetworkState::MAX,
                             SyncNetworkState::NETWORK_STATE_ONLINE);

BackgroundSyncServiceImpl::~BackgroundSyncServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_context_->background_sync_manager());
}

BackgroundSyncServiceImpl::BackgroundSyncServiceImpl(
    BackgroundSyncContextImpl* background_sync_context,
    mojo::InterfaceRequest<BackgroundSyncService> request)
    : background_sync_context_(background_sync_context),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_context);

  binding_.set_connection_error_handler(
      base::Bind(&BackgroundSyncServiceImpl::OnConnectionError,
                 base::Unretained(this) /* the channel is owned by this */));
}

void BackgroundSyncServiceImpl::OnConnectionError() {
  background_sync_context_->ServiceHadConnectionError(this);
  // |this| is now deleted.
}

void BackgroundSyncServiceImpl::Register(content::SyncRegistrationPtr options,
                                         int64_t sw_registration_id,
                                         bool requested_from_service_worker,
                                         const RegisterCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistrationOptions mgr_options =
      ToBackgroundSyncRegistrationOptions(options);

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->Register(
      sw_registration_id, mgr_options, requested_from_service_worker,
      base::Bind(&BackgroundSyncServiceImpl::OnRegisterResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::Unregister(
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    int64_t sw_registration_id,
    const UnregisterCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistrationHandle* registration =
      active_handles_.Lookup(handle_id);
  if (!registration) {
    callback.Run(BackgroundSyncError::NOT_ALLOWED);
    return;
  }

  registration->Unregister(
      sw_registration_id,
      base::Bind(&BackgroundSyncServiceImpl::OnUnregisterResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::GetRegistration(
    const mojo::String& tag,
    int64_t sw_registration_id,
    const GetRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->GetRegistration(
      sw_registration_id, tag.get(),
      base::Bind(&BackgroundSyncServiceImpl::OnRegisterResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::GetRegistrations(
    int64_t sw_registration_id,
    const GetRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->GetRegistrations(
      sw_registration_id,
      base::Bind(&BackgroundSyncServiceImpl::OnGetRegistrationsResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::GetPermissionStatus(
    int64_t sw_registration_id,
    const GetPermissionStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(iclelland): Implement a real policy. This is a stub implementation.
  // See https://crbug.com/482091.
  callback.Run(BackgroundSyncError::NONE, PermissionStatus::GRANTED);
}

void BackgroundSyncServiceImpl::DuplicateRegistrationHandle(
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    const DuplicateRegistrationHandleCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);

  scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle =
      background_sync_manager->DuplicateRegistrationHandle(handle_id);

  BackgroundSyncRegistrationHandle* handle_ptr = registration_handle.get();

  if (!registration_handle) {
    callback.Run(BackgroundSyncError::NOT_FOUND,
                 SyncRegistrationPtr(content::SyncRegistration::New()));
    return;
  }

  active_handles_.AddWithID(registration_handle.release(),
                            handle_ptr->handle_id());
  SyncRegistrationPtr mojoResult = ToMojoRegistration(*handle_ptr);
  callback.Run(BackgroundSyncError::NONE, std::move(mojoResult));
}

void BackgroundSyncServiceImpl::ReleaseRegistration(
    BackgroundSyncRegistrationHandle::HandleId handle_id) {
  if (!active_handles_.Lookup(handle_id)) {
    // TODO(jkarlin): Abort client.
    LOG(WARNING) << "Client attempted to release non-existing registration";
    return;
  }

  active_handles_.Remove(handle_id);
}

void BackgroundSyncServiceImpl::NotifyWhenFinished(
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    const NotifyWhenFinishedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncRegistrationHandle* registration =
      active_handles_.Lookup(handle_id);
  if (!registration) {
    callback.Run(BackgroundSyncError::NOT_ALLOWED, BackgroundSyncState::FAILED);
    return;
  }

  registration->NotifyWhenFinished(
      base::Bind(&BackgroundSyncServiceImpl::OnNotifyWhenFinishedResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::OnRegisterResult(
    const RegisterCallback& callback,
    BackgroundSyncStatus status,
    scoped_ptr<BackgroundSyncRegistrationHandle> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncRegistrationHandle* result_ptr = result.get();

  if (status != BACKGROUND_SYNC_STATUS_OK) {
    callback.Run(static_cast<content::BackgroundSyncError>(status),
                 SyncRegistrationPtr(content::SyncRegistration::New()));
    return;
  }

  DCHECK(result);
  active_handles_.AddWithID(result.release(), result_ptr->handle_id());
  SyncRegistrationPtr mojoResult = ToMojoRegistration(*result_ptr);
  callback.Run(static_cast<content::BackgroundSyncError>(status),
               std::move(mojoResult));
}

void BackgroundSyncServiceImpl::OnUnregisterResult(
    const UnregisterCallback& callback,
    BackgroundSyncStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(static_cast<content::BackgroundSyncError>(status));
}

void BackgroundSyncServiceImpl::OnGetRegistrationsResult(
    const GetRegistrationsCallback& callback,
    BackgroundSyncStatus status,
    scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>>
        result_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(result_registrations);

  mojo::Array<content::SyncRegistrationPtr> mojo_registrations(0);
  for (BackgroundSyncRegistrationHandle* registration : *result_registrations) {
    active_handles_.AddWithID(registration, registration->handle_id());
    mojo_registrations.push_back(ToMojoRegistration(*registration));
  }

  result_registrations->weak_clear();

  callback.Run(static_cast<content::BackgroundSyncError>(status),
               std::move(mojo_registrations));
}

void BackgroundSyncServiceImpl::OnNotifyWhenFinishedResult(
    const NotifyWhenFinishedCallback& callback,
    BackgroundSyncStatus status,
    BackgroundSyncState sync_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(static_cast<content::BackgroundSyncError>(status), sync_state);
}

}  // namespace content
