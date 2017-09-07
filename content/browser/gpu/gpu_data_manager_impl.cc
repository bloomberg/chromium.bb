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
  // Manually generate the following data instead of going through
  // gpu/config/process_json.py because this is just one simple instance.
  static const int kFeatureListForEntry0[1] = {
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL};
  static const gpu::GpuControlList::Entry kEntry = {
      1,  // id
      "ExtensionWebstoreGetWebGLStatusTest.Blocked",
      arraysize(kFeatureListForEntry0),  // features size
      kFeatureListForEntry0,             // features
      0,                                 // DisabledExtensions size
      nullptr,                           // DisabledExtensions
      0,                                 // CrBugs size
      nullptr,                           // CrBugs
      {
          gpu::GpuControlList::kOsAny,  // os_type
          {gpu::GpuControlList::kUnknown,
           gpu::GpuControlList::kVersionStyleNumerical, nullptr,
           nullptr},                                   // os_version
          0x00,                                        // vendor_id
          0,                                           // DeviceIDs size
          nullptr,                                     // DeviceIDs
          gpu::GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
          gpu::GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
          nullptr,                                     // driver info
          nullptr,                                     // GL strings
          nullptr,                                     // machine model info
          nullptr,                                     // more conditions
      },
      0,        // exceptions count
      nullptr,  // exceptions
  };
  static const gpu::GpuControlListData kData("1.0", 1, &kEntry);

  gpu::GPUInfo gpu_info;

  base::AutoLock auto_lock(lock_);
  private_->InitializeForTesting(kData, gpu_info);
}

void GpuDataManagerImpl::InitializeForTesting(
    const gpu::GpuControlListData& gpu_blacklist_data,
    const gpu::GPUInfo& gpu_info) {
  base::AutoLock auto_lock(lock_);
  private_->InitializeForTesting(gpu_blacklist_data, gpu_info);
}

bool GpuDataManagerImpl::IsFeatureBlacklisted(int feature) const {
  base::AutoLock auto_lock(lock_);
  return private_->IsFeatureBlacklisted(feature);
}

bool GpuDataManagerImpl::IsFeatureEnabled(int feature) const {
  base::AutoLock auto_lock(lock_);
  return private_->IsFeatureEnabled(feature);
}

bool GpuDataManagerImpl::IsWebGLEnabled() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsWebGLEnabled();
}

bool GpuDataManagerImpl::IsWebGL2Enabled() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsWebGL2Enabled();
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

bool GpuDataManagerImpl::IsCompleteGpuInfoAvailable() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsCompleteGpuInfoAvailable();
}

void GpuDataManagerImpl::RequestVideoMemoryUsageStatsUpdate(
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback) const {
  base::AutoLock auto_lock(lock_);
  private_->RequestVideoMemoryUsageStatsUpdate(callback);
}

bool GpuDataManagerImpl::ShouldUseSwiftShader() const {
  base::AutoLock auto_lock(lock_);
  return private_->ShouldUseSwiftShader();
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

void GpuDataManagerImpl::SetGLStrings(const std::string& gl_vendor,
                                      const std::string& gl_renderer,
                                      const std::string& gl_version) {
  base::AutoLock auto_lock(lock_);
  private_->SetGLStrings(gl_vendor, gl_renderer, gl_version);
}

void GpuDataManagerImpl::GetGLStrings(std::string* gl_vendor,
                                      std::string* gl_renderer,
                                      std::string* gl_version) {
  base::AutoLock auto_lock(lock_);
  private_->GetGLStrings(gl_vendor, gl_renderer, gl_version);
}

void GpuDataManagerImpl::DisableHardwareAcceleration() {
  base::AutoLock auto_lock(lock_);
  private_->DisableHardwareAcceleration();
}

bool GpuDataManagerImpl::HardwareAccelerationEnabled() const {
  base::AutoLock auto_lock(lock_);
  return !private_->ShouldUseSwiftShader() &&
         private_->GpuAccessAllowed(nullptr);
}

bool GpuDataManagerImpl::CanUseGpuBrowserCompositor() const {
  base::AutoLock auto_lock(lock_);
  return private_->CanUseGpuBrowserCompositor();
}

void GpuDataManagerImpl::GetDisabledExtensions(
    std::string* disabled_extensions) const {
  base::AutoLock auto_lock(lock_);
  private_->GetDisabledExtensions(disabled_extensions);
}

void GpuDataManagerImpl::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
  base::AutoLock auto_lock(lock_);
  private_->SetGpuInfo(gpu_info);
}

void GpuDataManagerImpl::Initialize() {
  base::AutoLock auto_lock(lock_);
  private_->Initialize();
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

void GpuDataManagerImpl::AppendRendererCommandLine(
    base::CommandLine* command_line) const {
  base::AutoLock auto_lock(lock_);
  private_->AppendRendererCommandLine(command_line);
}

void GpuDataManagerImpl::AppendGpuCommandLine(
    base::CommandLine* command_line) const {
  base::AutoLock auto_lock(lock_);
  private_->AppendGpuCommandLine(command_line);
}

void GpuDataManagerImpl::UpdateRendererWebPrefs(
    WebPreferences* prefs) const {
  base::AutoLock auto_lock(lock_);
  private_->UpdateRendererWebPrefs(prefs);
}

void GpuDataManagerImpl::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences) const {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuPreferences(gpu_preferences);
}

std::string GpuDataManagerImpl::GetBlacklistVersion() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetBlacklistVersion();
}

std::string GpuDataManagerImpl::GetDriverBugListVersion() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetDriverBugListVersion();
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

size_t GpuDataManagerImpl::GetBlacklistedFeatureCount() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetBlacklistedFeatureCount();
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
