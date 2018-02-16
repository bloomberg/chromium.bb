// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl.h"

#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "gpu/ipc/common/memory_stats.h"

namespace content {

// static
GpuDataManager* GpuDataManager::GetInstance() {
  return GpuDataManagerImpl::GetInstance();
}

// static
GpuDataManagerImpl* GpuDataManagerImpl::GetInstance() {
  return base::Singleton<GpuDataManagerImpl>::get();
}

void GpuDataManagerImpl::BlacklistWebGLForTesting() {
  base::AutoLock auto_lock(lock_);
  private_->BlacklistWebGLForTesting();
}

gpu::GPUInfo GpuDataManagerImpl::GetGPUInfo() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGPUInfo();
}

bool GpuDataManagerImpl::GpuAccessAllowed(std::string* reason) const {
  base::AutoLock auto_lock(lock_);
  return private_->GpuAccessAllowed(reason);
}

void GpuDataManagerImpl::RequestCompleteGpuInfoIfNeeded() {
  base::AutoLock auto_lock(lock_);
  private_->RequestCompleteGpuInfoIfNeeded();
}

bool GpuDataManagerImpl::IsEssentialGpuInfoAvailable() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsEssentialGpuInfoAvailable();
}

bool GpuDataManagerImpl::IsGpuFeatureInfoAvailable() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuFeatureInfoAvailable();
}

gpu::GpuFeatureStatus GpuDataManagerImpl::GetFeatureStatus(
    gpu::GpuFeatureType feature) const {
  base::AutoLock auto_lock(lock_);
  return private_->GetFeatureStatus(feature);
}

void GpuDataManagerImpl::RequestVideoMemoryUsageStatsUpdate(
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback) const {
  base::AutoLock auto_lock(lock_);
  private_->RequestVideoMemoryUsageStatsUpdate(callback);
}

void GpuDataManagerImpl::AddObserver(
    GpuDataManagerObserver* observer) {
  base::AutoLock auto_lock(lock_);
  private_->AddObserver(observer);
}

void GpuDataManagerImpl::RemoveObserver(
    GpuDataManagerObserver* observer) {
  base::AutoLock auto_lock(lock_);
  private_->RemoveObserver(observer);
}

void GpuDataManagerImpl::UnblockDomainFrom3DAPIs(const GURL& url) {
  base::AutoLock auto_lock(lock_);
  private_->UnblockDomainFrom3DAPIs(url);
}

void GpuDataManagerImpl::DisableHardwareAcceleration() {
  base::AutoLock auto_lock(lock_);
  private_->DisableHardwareAcceleration();
}

void GpuDataManagerImpl::BlockSwiftShader() {
  base::AutoLock auto_lock(lock_);
  private_->BlockSwiftShader();
}

bool GpuDataManagerImpl::HardwareAccelerationEnabled() const {
  base::AutoLock auto_lock(lock_);
  return private_->HardwareAccelerationEnabled();
}

void GpuDataManagerImpl::GetDisabledExtensions(
    std::string* disabled_extensions) const {
  base::AutoLock auto_lock(lock_);
  private_->GetDisabledExtensions(disabled_extensions);
}

void GpuDataManagerImpl::GetDisabledWebGLExtensions(
    std::string* disabled_webgl_extensions) const {
  base::AutoLock auto_lock(lock_);
  private_->GetDisabledWebGLExtensions(disabled_webgl_extensions);
}

void GpuDataManagerImpl::UpdateGpuInfo(const gpu::GPUInfo& gpu_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuInfo(gpu_info);
}

void GpuDataManagerImpl::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuFeatureInfo(gpu_feature_info);
}

gpu::GpuFeatureInfo GpuDataManagerImpl::GetGpuFeatureInfo() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGpuFeatureInfo();
}

void GpuDataManagerImpl::AppendGpuCommandLine(
    base::CommandLine* command_line) const {
  base::AutoLock auto_lock(lock_);
  private_->AppendGpuCommandLine(command_line);
}

void GpuDataManagerImpl::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences) const {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuPreferences(gpu_preferences);
}

void GpuDataManagerImpl::GetBlacklistReasons(base::ListValue* reasons) const {
  base::AutoLock auto_lock(lock_);
  private_->GetBlacklistReasons(reasons);
}

std::vector<std::string> GpuDataManagerImpl::GetDriverBugWorkarounds() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetDriverBugWorkarounds();
}

void GpuDataManagerImpl::AddLogMessage(int level,
                                       const std::string& header,
                                       const std::string& message) {
  base::AutoLock auto_lock(lock_);
  private_->AddLogMessage(level, header, message);
}

void GpuDataManagerImpl::ProcessCrashed(
    base::TerminationStatus exit_code) {
  base::AutoLock auto_lock(lock_);
  private_->ProcessCrashed(exit_code);
}

std::unique_ptr<base::ListValue> GpuDataManagerImpl::GetLogMessages() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetLogMessages();
}

void GpuDataManagerImpl::HandleGpuSwitch() {
  base::AutoLock auto_lock(lock_);
  private_->HandleGpuSwitch();
}

void GpuDataManagerImpl::BlockDomainFrom3DAPIs(
    const GURL& url, DomainGuilt guilt) {
  base::AutoLock auto_lock(lock_);
  private_->BlockDomainFrom3DAPIs(url, guilt);
}

bool GpuDataManagerImpl::Are3DAPIsBlocked(const GURL& top_origin_url,
                                          int render_process_id,
                                          int render_frame_id,
                                          ThreeDAPIType requester) {
  base::AutoLock auto_lock(lock_);
  return private_->Are3DAPIsBlocked(
      top_origin_url, render_process_id, render_frame_id, requester);
}

void GpuDataManagerImpl::DisableDomainBlockingFor3DAPIsForTesting() {
  base::AutoLock auto_lock(lock_);
  private_->DisableDomainBlockingFor3DAPIsForTesting();
}

bool GpuDataManagerImpl::UpdateActiveGpu(uint32_t vendor_id,
                                         uint32_t device_id) {
  base::AutoLock auto_lock(lock_);
  return private_->UpdateActiveGpu(vendor_id, device_id);
}

void GpuDataManagerImpl::Notify3DAPIBlocked(const GURL& top_origin_url,
                                            int render_process_id,
                                            int render_frame_id,
                                            ThreeDAPIType requester) {
  base::AutoLock auto_lock(lock_);
  private_->Notify3DAPIBlocked(
      top_origin_url, render_process_id, render_frame_id, requester);
}

void GpuDataManagerImpl::OnGpuProcessInitFailure() {
  base::AutoLock auto_lock(lock_);
  private_->OnGpuProcessInitFailure();
}

GpuDataManagerImpl::GpuDataManagerImpl()
    : private_(GpuDataManagerImplPrivate::Create(this)) {
}

GpuDataManagerImpl::~GpuDataManagerImpl() {
}

}  // namespace content
