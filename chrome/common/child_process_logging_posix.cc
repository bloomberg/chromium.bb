// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/common/gpu_info.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

// Account for the terminating null character.
static const size_t kMaxActiveURLSize = 1024 + 1;
static const size_t kClientIdSize = 32 + 1;
static const size_t kChannelSize = 32;

// We use static strings to hold the most recent active url and the client
// identifier. If we crash, the crash handler code will send the contents of
// these strings to the browser.
char g_active_url[kMaxActiveURLSize];
char g_client_id[kClientIdSize];

char g_channel[kChannelSize] = "";

static const size_t kGpuStringSize = 32;
char g_gpu_vendor_id[kGpuStringSize] = "";
char g_gpu_device_id[kGpuStringSize] = "";
char g_gpu_driver_ver[kGpuStringSize] = "";
char g_gpu_ps_ver[kGpuStringSize] = "";
char g_gpu_vs_ver[kGpuStringSize] = "";

char g_printer_info[kPrinterInfoStrLen * kMaxReportedPrinterRecords + 1] = "";

static const size_t kNumSize = 32;
char g_num_extensions[kNumSize] = "";
char g_num_switches[kNumSize] = "";
char g_num_views[kNumSize] = "";

static const size_t kMaxExtensionSize =
    kExtensionLen * kMaxReportedActiveExtensions + 1;
char g_extension_ids[kMaxExtensionSize] = "";

// Assume command line switches are less than 64 chars.
static const size_t kMaxSwitchesSize = kSwitchLen * kMaxSwitches + 1;
char g_switches[kMaxSwitchesSize] = "";

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
  snprintf(g_num_extensions, kNumSize - 1, "%" PRIuS, extension_ids.size());
  g_num_extensions[kNumSize - 1] = '\0';

  std::string extension_str;
  std::set<std::string>::const_iterator iter = extension_ids.begin();
  for (int i = 0;
       i < kMaxReportedActiveExtensions && iter != extension_ids.end();
       ++i, ++iter) {
    extension_str += *iter;
  }
  strncpy(g_extension_ids, extension_str.c_str(), kMaxExtensionSize - 1);
  g_extension_ids[kMaxExtensionSize - 1] = '\0';
}

void SetGpuInfo(const content::GPUInfo& gpu_info) {
  snprintf(g_gpu_vendor_id, kGpuStringSize, "0x%04x", gpu_info.gpu.vendor_id);
  snprintf(g_gpu_device_id, kGpuStringSize, "0x%04x", gpu_info.gpu.device_id);
  strncpy(g_gpu_driver_ver,
          gpu_info.driver_version.c_str(),
          kGpuStringSize - 1);
  g_gpu_driver_ver[kGpuStringSize - 1] = '\0';
  strncpy(g_gpu_ps_ver,
          gpu_info.pixel_shader_version.c_str(),
          kGpuStringSize - 1);
  g_gpu_ps_ver[kGpuStringSize - 1] = '\0';
  strncpy(g_gpu_vs_ver,
          gpu_info.vertex_shader_version.c_str(),
          kGpuStringSize - 1);
  g_gpu_vs_ver[kGpuStringSize - 1] = '\0';
}

void SetPrinterInfo(const char* printer_info) {
  std::string printer_info_str;
  std::vector<std::string> info;
  base::SplitString(printer_info, L';', &info);
  DCHECK_LE(info.size(), kMaxReportedPrinterRecords);
  for (size_t i = 0; i < info.size(); ++i) {
    printer_info_str += info[i];
    // Truncate long switches, align short ones with spaces to be trimmed later.
    printer_info_str.resize((i + 1) * kPrinterInfoStrLen, ' ');
  }
  strncpy(g_printer_info, printer_info_str.c_str(),
          arraysize(g_printer_info) - 1);
  g_printer_info[arraysize(g_printer_info) - 1] = '\0';
}

void SetNumberOfViews(int number_of_views) {
  snprintf(g_num_views, kNumSize - 1, "%d", number_of_views);
  g_num_views[kNumSize - 1] = '\0';
}

void SetCommandLine(const CommandLine* command_line) {
  const CommandLine::StringVector& argv = command_line->argv();

  snprintf(g_num_switches, kNumSize - 1, "%" PRIuS, argv.size() - 1);
  g_num_switches[kNumSize - 1] = '\0';

  std::string command_line_str;
  for (size_t argv_i = 1;
       argv_i < argv.size() && argv_i <= kMaxSwitches;
       ++argv_i) {
    command_line_str += argv[argv_i];
    // Truncate long switches, align short ones with spaces to be trimmed later.
    command_line_str.resize(argv_i * kSwitchLen, ' ');
  }
  strncpy(g_switches, command_line_str.c_str(), kMaxSwitchesSize - 1);
  g_switches[kMaxSwitchesSize - 1] = '\0';
}

void SetExperimentList(const std::vector<string16>& state) {
  // TODO(mad): Implement this.
}

void SetChannel(const std::string& channel) {
  strncpy(g_channel, channel.c_str(), kChannelSize - 1);
  g_channel[kChannelSize - 1] = '\0';
}

}  // namespace child_process_logging
