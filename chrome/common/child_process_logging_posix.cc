// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/metrics/variations/variations_util.h"
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
char g_gpu_gl_vendor[kGpuStringSize] = "";
char g_gpu_gl_renderer[kGpuStringSize] = "";
char g_gpu_driver_ver[kGpuStringSize] = "";
char g_gpu_ps_ver[kGpuStringSize] = "";
char g_gpu_vs_ver[kGpuStringSize] = "";

char g_printer_info[kPrinterInfoStrLen * kMaxReportedPrinterRecords + 1] = "";

static const size_t kNumSize = 32;
char g_num_extensions[kNumSize] = "";
char g_num_switches[kNumSize] = "";
char g_num_variations[kNumSize] = "";
char g_num_views[kNumSize] = "";

static const size_t kMaxExtensionSize =
    kExtensionLen * kMaxReportedActiveExtensions + 1;
char g_extension_ids[kMaxExtensionSize] = "";

// Assume command line switches are less than 64 chars.
static const size_t kMaxSwitchesSize = kSwitchLen * kMaxSwitches + 1;
char g_switches[kMaxSwitchesSize] = "";

static const size_t kMaxVariationChunksSize =
    kMaxVariationChunkSize * kMaxReportedVariationChunks + 1;
char g_variation_chunks[kMaxVariationChunksSize] = "";

void SetActiveURL(const GURL& url) {
  base::strlcpy(g_active_url, url.possibly_invalid_spec().c_str(),
                arraysize(g_active_url));
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
  snprintf(g_num_extensions, arraysize(g_num_extensions), "%" PRIuS,
           extension_ids.size());

  std::string extension_str;
  std::set<std::string>::const_iterator iter = extension_ids.begin();
  for (size_t i = 0;
       i < kMaxReportedActiveExtensions && iter != extension_ids.end();
       ++i, ++iter) {
    extension_str += *iter;
  }
  base::strlcpy(g_extension_ids, extension_str.c_str(),
                arraysize(g_extension_ids));
}

void SetGpuInfo(const content::GPUInfo& gpu_info) {
  snprintf(g_gpu_vendor_id, arraysize(g_gpu_vendor_id), "0x%04x",
           gpu_info.gpu.vendor_id);
  snprintf(g_gpu_device_id, arraysize(g_gpu_device_id), "0x%04x",
           gpu_info.gpu.device_id);
  base::strlcpy(g_gpu_gl_vendor, gpu_info.gl_vendor.c_str(),
                arraysize(g_gpu_gl_vendor));
  base::strlcpy(g_gpu_gl_renderer, gpu_info.gl_renderer.c_str(),
                arraysize(g_gpu_gl_renderer));
  base::strlcpy(g_gpu_driver_ver, gpu_info.driver_version.c_str(),
                arraysize(g_gpu_driver_ver));
  base::strlcpy(g_gpu_ps_ver, gpu_info.pixel_shader_version.c_str(),
                arraysize(g_gpu_ps_ver));
  base::strlcpy(g_gpu_vs_ver,  gpu_info.vertex_shader_version.c_str(),
                arraysize(g_gpu_vs_ver));
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
  base::strlcpy(g_printer_info, printer_info_str.c_str(),
                arraysize(g_printer_info));
}

void SetNumberOfViews(int number_of_views) {
  snprintf(g_num_views, arraysize(g_num_views), "%d", number_of_views);
}

void SetCommandLine(const CommandLine* command_line) {
  const CommandLine::StringVector& argv = command_line->argv();

  snprintf(g_num_switches, arraysize(g_num_switches), "%" PRIuS,
           argv.size() - 1);

  std::string command_line_str;
  for (size_t argv_i = 1;
       argv_i < argv.size() && argv_i <= kMaxSwitches;
       ++argv_i) {
    command_line_str += argv[argv_i];
    // Truncate long switches, align short ones with spaces to be trimmed later.
    command_line_str.resize(argv_i * kSwitchLen, ' ');
  }
  base::strlcpy(g_switches, command_line_str.c_str(), arraysize(g_switches));
}

void SetExperimentList(const std::vector<string16>& experiments) {
  std::vector<string16> chunks;
  chrome_variations::GenerateVariationChunks(experiments, &chunks);

  // Store up to |kMaxReportedVariationChunks| chunks.
  std::string chunks_str;
  const size_t number_of_chunks_to_report =
      std::min(chunks.size(), kMaxReportedVariationChunks);
  for (size_t i = 0; i < number_of_chunks_to_report; ++i) {
    chunks_str += UTF16ToUTF8(chunks[i]);
    // Align short chunks with spaces to be trimmed later.
    chunks_str.resize(i * kMaxVariationChunkSize, ' ');
  }
  base::strlcpy(g_variation_chunks, chunks_str.c_str(),
                arraysize(g_variation_chunks));

  // Make note of the total number of experiments, which may be greater than
  // what was able to fit in |kMaxReportedVariationChunks|. This is useful when
  // correlating stability with the number of experiments running
  // simultaneously.
  snprintf(g_num_variations, arraysize(g_num_variations), "%" PRIuS,
           experiments.size());
}

void SetChannel(const std::string& channel) {
  base::strlcpy(g_channel, channel.c_str(), arraysize(g_channel));
}

}  // namespace child_process_logging
