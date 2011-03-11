// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/common/gpu_info.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

// Account for the terminating null character.
static const size_t kMaxActiveURLSize = 1024 + 1;
static const size_t kClientIdSize = 32 + 1;

// We use static strings to hold the most recent active url and the client
// identifier. If we crash, the crash handler code will send the contents of
// these strings to the browser.
char g_active_url[kMaxActiveURLSize];
char g_client_id[kClientIdSize];

static const size_t kGpuStringSize = 32;

char g_gpu_vendor_id[kGpuStringSize] = "";
char g_gpu_device_id[kGpuStringSize] = "";
char g_gpu_driver_ver[kGpuStringSize] = "";
char g_gpu_ps_ver[kGpuStringSize] = "";
char g_gpu_vs_ver[kGpuStringSize] = "";

void SetActiveURL(const GURL& url) {
  base::strlcpy(g_active_url,
                url.possibly_invalid_spec().c_str(),
                kMaxActiveURLSize);
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  if (str.empty())
    return;

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

void SetActiveExtensions(const std::set<std::string>& extension_ids) {
  // TODO(port)
}

void SetGpuInfo(const GPUInfo& gpu_info) {
  snprintf(g_gpu_vendor_id, kGpuStringSize, "0x%04x", gpu_info.vendor_id);
  snprintf(g_gpu_device_id, kGpuStringSize, "0x%04x", gpu_info.device_id);
  strncpy(g_gpu_driver_ver,
          gpu_info.driver_version.c_str(),
          kGpuStringSize - 1);
  g_gpu_driver_ver[kGpuStringSize - 1] = '\0';
  strncpy(g_gpu_ps_ver,
          base::UintToString(gpu_info.pixel_shader_version).c_str(),
          kGpuStringSize - 1);
  g_gpu_ps_ver[kGpuStringSize - 1] = '\0';
  strncpy(g_gpu_vs_ver,
          base::UintToString(gpu_info.vertex_shader_version).c_str(),
          kGpuStringSize - 1);
  g_gpu_vs_ver[kGpuStringSize - 1] = '\0';
}

void SetNumberOfViews(int number_of_views) {
  // TODO(port)
}

}  // namespace child_process_logging
