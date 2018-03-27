// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_memory_details.h"

#include <stddef.h>

#include <vector>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/process_type.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

MetricsMemoryDetails::MetricsMemoryDetails(
    const base::Closure& callback)
    : callback_(callback),
      generate_histograms_(true) {}

MetricsMemoryDetails::~MetricsMemoryDetails() {
}

void MetricsMemoryDetails::OnDetailsAvailable() {
  if (generate_histograms_)
    UpdateHistograms();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback_);
}

void MetricsMemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.

  const ProcessData& browser = *ChromeBrowser();
  int chrome_count = 0;
  int extension_count = 0;
  int renderer_count = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int num_open_fds = browser.processes[index].num_open_fds;
    int open_fds_soft_limit = browser.processes[index].open_fds_soft_limit;
    switch (browser.processes[index].process_type) {
      case content::PROCESS_TYPE_BROWSER:
        if (num_open_fds != -1 && open_fds_soft_limit != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.Browser.OpenFDs", num_open_fds);
          UMA_HISTOGRAM_COUNTS_10000("Memory.Browser.OpenFDsSoftLimit",
                                     open_fds_soft_limit);
        }
        continue;
      case content::PROCESS_TYPE_RENDERER: {
        if (num_open_fds != -1 && open_fds_soft_limit != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.RendererAll.OpenFDs",
                                     num_open_fds);
          UMA_HISTOGRAM_COUNTS_10000("Memory.RendererAll.OpenFDsSoftLimit",
                                     open_fds_soft_limit);
        }
        ProcessMemoryInformation::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ProcessMemoryInformation::RENDERER_EXTENSION:
            if (num_open_fds != -1) {
              UMA_HISTOGRAM_COUNTS_10000("Memory.Extension.OpenFDs",
                                         num_open_fds);
            }
            extension_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            if (num_open_fds != -1)
              UMA_HISTOGRAM_COUNTS_10000("Memory.Chrome.OpenFDs", num_open_fds);
            chrome_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            continue;
          case ProcessMemoryInformation::RENDERER_NORMAL:
          default:
            if (num_open_fds != -1) {
              UMA_HISTOGRAM_COUNTS_10000("Memory.Renderer.OpenFDs",
                                         num_open_fds);
            }
            renderer_count++;
            continue;
        }
      }
      case content::PROCESS_TYPE_UTILITY:
        if (num_open_fds != -1)
          UMA_HISTOGRAM_COUNTS_10000("Memory.Utility.OpenFDs", num_open_fds);
        continue;
      case content::PROCESS_TYPE_ZYGOTE:
        if (num_open_fds != -1)
          UMA_HISTOGRAM_COUNTS_10000("Memory.Zygote.OpenFDs", num_open_fds);
        continue;
      case content::PROCESS_TYPE_SANDBOX_HELPER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.SandboxHelper.OpenFDs",
                                     num_open_fds);
        }
        continue;
      case content::PROCESS_TYPE_GPU:
        if (num_open_fds != -1 && open_fds_soft_limit != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.Gpu.OpenFDs", num_open_fds);
          UMA_HISTOGRAM_COUNTS_10000("Memory.Gpu.OpenFDsSoftLimit",
                                     open_fds_soft_limit);
        }
        continue;
#if BUILDFLAG(ENABLE_PLUGINS)
      case content::PROCESS_TYPE_PPAPI_PLUGIN: {
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.PepperPlugin.OpenFDs",
                                     num_open_fds);
        }
        continue;
      }
      case content::PROCESS_TYPE_PPAPI_BROKER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.PepperPluginBroker.OpenFDs",
                                     num_open_fds);
        }
        continue;
#endif
      case PROCESS_TYPE_NACL_LOADER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.NativeClient.OpenFDs",
                                     num_open_fds);
        }
        continue;
      case PROCESS_TYPE_NACL_BROKER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.NativeClientBroker.OpenFDs",
                                     num_open_fds);
        }
        continue;
      default:
        NOTREACHED();
        continue;
    }
  }
#if defined(OS_CHROMEOS)
  // Chrome OS exposes system-wide graphics driver memory which has historically
  // been a source of leak/bloat.
  base::SystemMemoryInfoKB meminfo;
  if (base::GetSystemMemoryInfo(&meminfo) && meminfo.gem_size != -1)
    UMA_HISTOGRAM_MEMORY_MB("Memory.Graphics", meminfo.gem_size / 1024 / 1024);
#endif

  // Predict the number of processes needed when isolating all sites and when
  // isolating only HTTPS sites.
  int all_renderer_count = renderer_count + chrome_count + extension_count;
  int non_renderer_count = browser.processes.size() - all_renderer_count;
  DCHECK_GE(non_renderer_count, 1);
  SiteDetails::UpdateHistograms(browser.site_data, all_renderer_count,
                                non_renderer_count);

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
                           static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.ExtensionProcessCount", extension_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.RendererProcessCount", renderer_count);

  leveldb_chrome::UpdateHistograms();
}
