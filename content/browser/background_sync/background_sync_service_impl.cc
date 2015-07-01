// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl.h"

#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// TODO(iclelland): Move these converters to mojo::TypeConverter template
// specializations.

scoped_ptr<BackgroundSyncManager::BackgroundSyncRegistration>
ToBackgroundSyncRegistration(const SyncRegistrationPtr& in) {
  scoped_ptr<BackgroundSyncManager::BackgroundSyncRegistration> out(
      new BackgroundSyncManager::BackgroundSyncRegistration());
  out->tag = in->tag;
  out->id = in->id;
  out->min_period = in->min_period_ms;
  out->power_state = static_cast<SyncPowerState>(in->power_state);
  out->network_state = static_cast<SyncNetworkState>(in->network_state);
  out->periodicity = static_cast<SyncPeriodicity>(in->periodicity);
  return out;
}

SyncRegistrationPtr ToMojoRegistration(
    const BackgroundSyncManager::BackgroundSyncRegistration& in) {
  SyncRegistrationPtr out(content::SyncRegistration::New());
  out->id = in.id;
  out->tag = in.tag;
  out->min_period_ms = in.min_period;
  out->periodicity =
      static_cast<content::BackgroundSyncPeriodicity>(in.periodicity);
  out->power_state =
      static_cast<content::BackgroundSyncPowerState>(in.power_state);
  out->network_state =
      static_cast<content::BackgroundSyncNetworkState>(in.network_state);
  return out.Pass();
}

}  // namespace

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, manager_name) \
  COMPILE_ASSERT(static_cast<int>(content::mojo_name) ==      \
                     static_cast<int>(content::manager_name), \
                 mismatching_enums)

// TODO(iclelland): Move these tests somewhere else
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_ERROR_NONE,
                             BackgroundSyncManager::ERROR_TYPE_OK);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_ERROR_STORAGE,
                             BackgroundSyncManager::ERROR_TYPE_STORAGE);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_ERROR_NOT_FOUND,
                             BackgroundSyncManager::ERROR_TYPE_NOT_FOUND);
COMPILE_ASSERT_MATCHING_ENUM(
    BACKGROUND_SYNC_ERROR_NO_SERVICE_WORKER,
    BackgroundSyncManager::ERROR_TYPE_NO_SERVICE_WORKER);
COMPILE_ASSERT_MATCHING_ENUM(
    BACKGROUND_SYNC_ERROR_MAX,
    BackgroundSyncManager::ERROR_TYPE_NO_SERVICE_WORKER);

COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_ANY,
                             SyncNetworkState::NETWORK_STATE_ANY);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR,
                             SyncNetworkState::NETWORK_STATE_AVOID_CELLULAR);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_ONLINE,
                             SyncNetworkState::NETWORK_STATE_ONLINE);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_MAX,
                             SyncNetworkState::NETWORK_STATE_ONLINE);

COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_AUTO,
                             SyncPowerState::POWER_STATE_AUTO);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING,
                             SyncPowerState::POWER_STATE_AVOID_DRAINING);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_MAX,
                             SyncPowerState::POWER_STATE_AVOID_DRAINING);

COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_PERIODIC,
                             SyncPeriodicity::SYNC_PERIODIC);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_ONE_SHOT,
                             SyncPeriodicity::SYNC_ONE_SHOT);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_MAX,
                             SyncPeriodicity::SYNC_ONE_SHOT);

// static
void BackgroundSyncServiceImpl::Create(
    const scoped_refptr<BackgroundSyncContextImpl>& background_sync_context,
    mojo::InterfaceRequest<BackgroundSyncService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BackgroundSyncServiceImpl::CreateOnIOThread,
                 background_sync_context, base::Passed(&request)));
}

BackgroundSyncServiceImpl::~BackgroundSyncServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
void BackgroundSyncServiceImpl::CreateOnIOThread(
    const scoped_refptr<BackgroundSyncContextImpl>& background_sync_context,
    mojo::InterfaceRequest<BackgroundSyncService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  new BackgroundSyncServiceImpl(background_sync_context, request.Pass());
}

BackgroundSyncServiceImpl::BackgroundSyncServiceImpl(
    const scoped_refptr<BackgroundSyncContextImpl>& background_sync_context,
    mojo::InterfaceRequest<BackgroundSyncService> request)
    : background_sync_context_(background_sync_context),
      binding_(this, request.Pass()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundSyncServiceImpl::Register(content::SyncRegistrationPtr options,
                                         int64_t sw_registration_id,
                                         const RegisterCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_ptr<BackgroundSyncManager::BackgroundSyncRegistration> reg =
      ToBackgroundSyncRegistration(options);

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->Register(
      sw_registration_id, *(reg.get()),
      base::Bind(&BackgroundSyncServiceImpl::OnRegisterResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::Unregister(
    BackgroundSyncPeriodicity periodicity,
    int64_t id,
    const mojo::String& tag,
    int64_t sw_registration_id,
    const UnregisterCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->Unregister(
      sw_registration_id, tag, content::SYNC_ONE_SHOT, id,
      base::Bind(&BackgroundSyncServiceImpl::OnUnregisterResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::GetRegistration(
    BackgroundSyncPeriodicity periodicity,
    const mojo::String& tag,
    int64_t sw_registration_id,
    const GetRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->GetRegistration(
      sw_registration_id, tag.get(), static_cast<SyncPeriodicity>(periodicity),
      base::Bind(&BackgroundSyncServiceImpl::OnRegisterResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::GetRegistrations(
    BackgroundSyncPeriodicity periodicity,
    int64_t sw_registration_id,
    const GetRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->GetRegistrations(
      sw_registration_id, static_cast<SyncPeriodicity>(periodicity),
      base::Bind(&BackgroundSyncServiceImpl::OnGetRegistrationsResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncServiceImpl::GetPermissionStatus(
    BackgroundSyncPeriodicity periodicity,
    int64_t sw_registration_id,
    const GetPermissionStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(iclelland): Implement a real policy. This is a stub implementation.
  // OneShot: crbug.com/482091
  // Periodic: crbug.com/482093
  callback.Run(BACKGROUND_SYNC_ERROR_NONE, PERMISSION_STATUS_GRANTED);
}

void BackgroundSyncServiceImpl::OnRegisterResult(
    const RegisterCallback& callback,
    BackgroundSyncManager::ErrorType error,
    const BackgroundSyncManager::BackgroundSyncRegistration& result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SyncRegistrationPtr mojoResult = ToMojoRegistration(result);
  callback.Run(static_cast<content::BackgroundSyncError>(error),
               mojoResult.Pass());
}

void BackgroundSyncServiceImpl::OnUnregisterResult(
    const UnregisterCallback& callback,
    BackgroundSyncManager::ErrorType error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(static_cast<content::BackgroundSyncError>(error));
}

void BackgroundSyncServiceImpl::OnGetRegistrationsResult(
    const GetRegistrationsCallback& callback,
    BackgroundSyncManager::ErrorType error,
    const std::vector<BackgroundSyncManager::BackgroundSyncRegistration>&
        result_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::Array<content::SyncRegistrationPtr> mojo_registrations(0);
  for (const auto& registration : result_registrations)
    mojo_registrations.push_back(ToMojoRegistration(registration));
  callback.Run(static_cast<content::BackgroundSyncError>(error),
               mojo_registrations.Pass());
}

}  // namespace content
