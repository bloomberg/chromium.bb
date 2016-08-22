// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/gpu_driver_info_manager_android.h"

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info_collector.h"

using base::android::BuildInfo;

// static
void GpuProfileCache::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGLVendorString, std::string());
  registry->RegisterStringPref(prefs::kGLVersionString, std::string());
  registry->RegisterStringPref(prefs::kGLRendererString, std::string());
  registry->RegisterStringPref(prefs::kGLExtensionsString, std::string());
  registry->RegisterStringPref(prefs::kGpuDriverInfoMaxSamples, std::string());
  registry->RegisterIntegerPref(
      prefs::kGpuDriverInfoResetNotificationStrategy, 0);
  registry->RegisterStringPref(
      prefs::kGpuDriverInfoShaderVersion, std::string());
  registry->RegisterStringPref(
      prefs::kGpuDriverInfoBuildFingerPrint, std::string());
}

// static
GpuProfileCache* GpuProfileCache::Create() {
  return new GpuDriverInfoManager();
}

GpuDriverInfoManager::GpuDriverInfoManager() {}

GpuDriverInfoManager::~GpuDriverInfoManager() {}

void GpuDriverInfoManager::Initialize() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  std::string build_finger_print =
      local_state->GetString(prefs::kGpuDriverInfoBuildFingerPrint);
  if (build_finger_print.empty() || build_finger_print !=
      BuildInfo::GetInstance()->android_build_fp()) {
    content::GpuDataManager::GetInstance()->AddObserver(this);
    return;
  }

  gpu::GPUInfo gpu_info;
  gpu_info.gl_vendor = local_state->GetString(prefs::kGLVendorString);
  gpu_info.gl_version = local_state->GetString(prefs::kGLVersionString);
  gpu_info.gl_renderer = local_state->GetString(prefs::kGLRendererString);
  gpu_info.gl_extensions = local_state->GetString(
      prefs::kGLExtensionsString);
  gpu_info.max_msaa_samples = local_state->GetString(
      prefs::kGpuDriverInfoMaxSamples);
  gpu_info.gl_reset_notification_strategy = local_state->GetInteger(
      prefs::kGpuDriverInfoResetNotificationStrategy);
  std::string shader_version = local_state->GetString(
      prefs::kGpuDriverInfoShaderVersion);
  gpu_info.pixel_shader_version = shader_version;
  gpu_info.vertex_shader_version = shader_version;
  gpu_info.machine_model_name = BuildInfo::GetInstance()->model();

  gpu::CollectInfoResult result = gpu::CollectDriverInfoGL(&gpu_info);
  gpu_info.basic_info_state = result;
  gpu_info.context_info_state = result;
  content::GpuDataManager::GetInstance()->SetGpuInfo(gpu_info);
}

void GpuDriverInfoManager::OnGpuInfoUpdate() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  local_state->SetString(prefs::kGLVendorString, gpu_info.gl_vendor);
  local_state->SetString(prefs::kGLVersionString, gpu_info.gl_version);
  local_state->SetString(prefs::kGLRendererString, gpu_info.gl_renderer);
  local_state->SetString(
      prefs::kGLExtensionsString, gpu_info.gl_extensions);
  local_state->SetString(
      prefs::kGpuDriverInfoMaxSamples, gpu_info.max_msaa_samples);
  local_state->SetInteger(prefs::kGpuDriverInfoResetNotificationStrategy,
                          gpu_info.gl_reset_notification_strategy);
  local_state->SetString(prefs::kGpuDriverInfoShaderVersion,
                         gpu_info.pixel_shader_version);
  local_state->SetString(prefs::kGpuDriverInfoBuildFingerPrint,
                         BuildInfo::GetInstance()->android_build_fp());
  content::GpuDataManager::GetInstance()->RemoveObserver(this);
}

