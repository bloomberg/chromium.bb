// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "content/public/browser/zygote_host_linux.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#endif

using base::StringPrintf;
using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::NavigationEntry;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
#if defined(ENABLE_EXTENSIONS)
using extensions::Extension;
#endif

// static
std::string ProcessMemoryInformation::GetRendererTypeNameInEnglish(
    RendererProcessType type) {
  switch (type) {
    case RENDERER_NORMAL:
      return "Tab";
    case RENDERER_CHROME:
      return "Tab (Chrome)";
    case RENDERER_EXTENSION:
      return "Extension";
    case RENDERER_DEVTOOLS:
      return "Devtools";
    case RENDERER_INTERSTITIAL:
      return "Interstitial";
    case RENDERER_BACKGROUND_APP:
      return "Background App";
    case RENDERER_UNKNOWN:
    default:
      NOTREACHED() << "Unknown renderer process type!";
      return "Unknown";
  }
}

// static
std::string ProcessMemoryInformation::GetFullTypeNameInEnglish(
    int process_type,
    RendererProcessType rtype) {
  if (process_type == content::PROCESS_TYPE_RENDERER)
    return GetRendererTypeNameInEnglish(rtype);
  return content::GetProcessTypeNameInEnglish(process_type);
}

ProcessMemoryInformation::ProcessMemoryInformation()
    : pid(0),
      num_processes(0),
      is_diagnostics(false),
      process_type(content::PROCESS_TYPE_UNKNOWN),
      renderer_type(RENDERER_UNKNOWN) {
}

ProcessMemoryInformation::~ProcessMemoryInformation() {}

bool ProcessMemoryInformation::operator<(
    const ProcessMemoryInformation& rhs) const {
  return working_set.priv < rhs.working_set.priv;
}

ProcessData::ProcessData() {}

ProcessData::ProcessData(const ProcessData& rhs)
    : name(rhs.name),
      process_name(rhs.process_name),
      processes(rhs.processes) {
}

ProcessData::~ProcessData() {}

ProcessData& ProcessData::operator=(const ProcessData& rhs) {
  name = rhs.name;
  process_name = rhs.process_name;
  processes = rhs.processes;
  return *this;
}

MemoryGrowthTracker::MemoryGrowthTracker() {}

MemoryGrowthTracker::~MemoryGrowthTracker() {}

bool MemoryGrowthTracker::UpdateSample(
    base::ProcessId pid,
    int sample,
    int* diff) {
  // |sample| is memory usage in kB.
  const base::TimeTicks current_time = base::TimeTicks::Now();
  std::map<base::ProcessId, int>::iterator found_size = memory_sizes_.find(pid);
  if (found_size != memory_sizes_.end()) {
    const int last_size = found_size->second;
    std::map<base::ProcessId, base::TimeTicks>::iterator found_time =
        times_.find(pid);
    const base::TimeTicks last_time = found_time->second;
    if (last_time < (current_time - base::TimeDelta::FromMinutes(30))) {
      // Note that it is undefined how division of a negative integer gets
      // rounded. |*diff| may have a difference of 1 from the correct number
      // if |sample| < |last_size|. We ignore it as 1 is small enough.
      *diff = ((sample - last_size) * 30 /
               (current_time - last_time).InMinutes());
      found_size->second = sample;
      found_time->second = current_time;
      return true;
    }
    // Skip if a last record is found less than 30 minutes ago.
  } else {
    // Not reporting if it's the first record for |pid|.
    times_[pid] = current_time;
    memory_sizes_[pid] = sample;
  }
  return false;
}

// About threading:
//
// This operation will hit no fewer than 3 threads.
//
// The BrowserChildProcessHostIterator can only be accessed from the IO thread.
//
// The RenderProcessHostIterator can only be accessed from the UI thread.
//
// This operation can take 30-100ms to complete.  We never want to have
// one task run for that long on the UI or IO threads.  So, we run the
// expensive parts of this operation over on the file thread.
//
void MemoryDetails::StartFetch(UserMetricsMode user_metrics_mode) {
  // This might get called from the UI or FILE threads, but should not be
  // getting called from the IO thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  user_metrics_mode_ = user_metrics_mode;

  // In order to process this request, we need to use the plugin information.
  // However, plugin process information is only available from the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnIOThread, this));
}

MemoryDetails::~MemoryDetails() {}

std::string MemoryDetails::ToLogString() {
  std::string log;
  log.reserve(4096);
  ProcessMemoryInformationList processes = ChromeBrowser()->processes;
  // Sort by memory consumption, low to high.
  std::sort(processes.begin(), processes.end());
  // Print from high to low.
  for (ProcessMemoryInformationList::reverse_iterator iter1 =
          processes.rbegin();
       iter1 != processes.rend();
       ++iter1) {
    log += ProcessMemoryInformation::GetFullTypeNameInEnglish(
            iter1->process_type, iter1->renderer_type);
    if (!iter1->titles.empty()) {
      log += " [";
      for (std::vector<base::string16>::const_iterator iter2 =
               iter1->titles.begin();
           iter2 != iter1->titles.end(); ++iter2) {
        if (iter2 != iter1->titles.begin())
          log += "|";
        log += base::UTF16ToUTF8(*iter2);
      }
      log += "]";
    }
    log += StringPrintf(" %d MB private, %d MB shared",
                        static_cast<int>(iter1->working_set.priv) / 1024,
                        static_cast<int>(iter1->working_set.shared) / 1024);
#if defined(OS_CHROMEOS)
    log += StringPrintf(", %d MB swapped",
                        static_cast<int>(iter1->working_set.swapped) / 1024);
#endif
    log += "\n";
  }
  return log;
}

void MemoryDetails::SetMemoryGrowthTracker(
    MemoryGrowthTracker* memory_growth_tracker) {
  memory_growth_tracker_ = memory_growth_tracker;
}

void MemoryDetails::CollectChildInfoOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<ProcessMemoryInformation> child_info;

  // Collect the list of child processes. A 0 |handle| means that
  // the process is being launched, so we skip it.
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    ProcessMemoryInformation info;
    if (!iter.GetData().handle)
      continue;
    info.pid = base::GetProcId(iter.GetData().handle);
    if (!info.pid)
      continue;

    info.process_type = iter.GetData().process_type;
    info.renderer_type = ProcessMemoryInformation::RENDERER_UNKNOWN;
    info.titles.push_back(iter.GetData().name);
    child_info.push_back(info);
  }

  // Now go do expensive memory lookups from the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MemoryDetails::CollectProcessData, this, child_info));
}

void MemoryDetails::CollectChildInfoOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  const pid_t zygote_pid = content::ZygoteHost::GetInstance()->GetPid();
#endif

  ProcessData* const chrome_browser = ChromeBrowser();
  // Get more information about the process.
  for (size_t index = 0; index < chrome_browser->processes.size();
      index++) {
    // Check if it's a renderer, if so get the list of page titles in it and
    // check if it's a diagnostics-related process.  We skip about:memory pages.
    // Iterate the RenderProcessHosts to find the tab contents.
    ProcessMemoryInformation& process =
        chrome_browser->processes[index];

    scoped_ptr<content::RenderWidgetHostIterator> widgets(
        RenderWidgetHost::GetRenderWidgetHosts());
    while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
      content::RenderProcessHost* render_process_host =
          widget->GetProcess();
      DCHECK(render_process_host);
      // Ignore processes that don't have a connection, such as crashed tabs.
      if (!render_process_host->HasConnection() ||
          process.pid != base::GetProcId(render_process_host->GetHandle())) {
        continue;
      }

      // The RenderProcessHost may host multiple WebContentses.  Any
      // of them which contain diagnostics information make the whole
      // process be considered a diagnostics process.
      if (!widget->IsRenderView())
        continue;

      process.process_type = content::PROCESS_TYPE_RENDERER;
      bool is_extension = false;
      RenderViewHost* host = RenderViewHost::From(widget);
#if defined(ENABLE_EXTENSIONS)
      content::BrowserContext* context =
          render_process_host->GetBrowserContext();
      ExtensionService* extension_service =
          extensions::ExtensionSystem::Get(context)->extension_service();
      extensions::ProcessMap* extension_process_map =
          extensions::ProcessMap::Get(context);
      is_extension = extension_process_map->Contains(
          host->GetProcess()->GetID());
#endif

      WebContents* contents = WebContents::FromRenderViewHost(host);
      GURL url;
      if (contents) {
        url = contents->GetURL();
        SiteData* site_data =
            &chrome_browser->site_data[contents->GetBrowserContext()];
        SiteDetails::CollectSiteInfo(contents, site_data);
      }
#if defined(ENABLE_EXTENSIONS)
      extensions::ViewType type = extensions::GetViewType(contents);
#endif
      if (host->GetEnabledBindings() & content::BINDINGS_POLICY_WEB_UI) {
        process.renderer_type = ProcessMemoryInformation::RENDERER_CHROME;
      } else if (is_extension) {
#if defined(ENABLE_EXTENSIONS)
        // For our purposes, don't count processes containing only hosted apps
        // as extension processes. See also: crbug.com/102533.
        std::set<std::string> extension_ids =
            extension_process_map->GetExtensionsInProcess(
            host->GetProcess()->GetID());
        for (std::set<std::string>::iterator iter = extension_ids.begin();
             iter != extension_ids.end(); ++iter) {
          const Extension* extension =
              extension_service->GetExtensionById(*iter, false);
          if (extension && !extension->is_hosted_app()) {
            process.renderer_type =
                ProcessMemoryInformation::RENDERER_EXTENSION;
            break;
          }
        }
#endif
      }
#if defined(ENABLE_EXTENSIONS)
      if (is_extension) {
        const Extension* extension =
            extension_service->extensions()->GetByID(url.host());
        if (extension) {
          base::string16 title = base::UTF8ToUTF16(extension->name());
          process.titles.push_back(title);
          process.renderer_type =
              ProcessMemoryInformation::RENDERER_EXTENSION;
          continue;
        }
      }
#endif

      if (!contents) {
        process.renderer_type =
            ProcessMemoryInformation::RENDERER_INTERSTITIAL;
        continue;
      }

#if defined(ENABLE_EXTENSIONS)
      if (type == extensions::VIEW_TYPE_BACKGROUND_CONTENTS) {
        process.titles.push_back(base::UTF8ToUTF16(url.spec()));
        process.renderer_type =
            ProcessMemoryInformation::RENDERER_BACKGROUND_APP;
        continue;
      }
#endif

      // Since we have a WebContents and and the renderer type hasn't been
      // set yet, it must be a normal tabbed renderer.
      if (process.renderer_type == ProcessMemoryInformation::RENDERER_UNKNOWN)
        process.renderer_type = ProcessMemoryInformation::RENDERER_NORMAL;

      base::string16 title = contents->GetTitle();
      if (!title.length())
        title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
      process.titles.push_back(title);

      // We need to check the pending entry as well as the virtual_url to
      // see if it's a chrome://memory URL (we don't want to count these in
      // the total memory usage of the browser).
      //
      // When we reach here, chrome://memory will be the pending entry since
      // we haven't responded with any data such that it would be committed.
      // If you have another chrome://memory tab open (which would be
      // committed), we don't want to count it either, so we also check the
      // last committed entry.
      //
      // Either the pending or last committed entries can be NULL.
      const NavigationEntry* pending_entry =
          contents->GetController().GetPendingEntry();
      const NavigationEntry* last_committed_entry =
          contents->GetController().GetLastCommittedEntry();
      if ((last_committed_entry &&
           LowerCaseEqualsASCII(last_committed_entry->GetVirtualURL().spec(),
                                chrome::kChromeUIMemoryURL)) ||
          (pending_entry &&
           LowerCaseEqualsASCII(pending_entry->GetVirtualURL().spec(),
                                chrome::kChromeUIMemoryURL))) {
        process.is_diagnostics = true;
      }
    }

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    if (process.pid == zygote_pid) {
      process.process_type = content::PROCESS_TYPE_ZYGOTE;
    }
#endif
  }

  // Get rid of other Chrome processes that are from a different profile.
  for (size_t index = 0; index < chrome_browser->processes.size();
      index++) {
    if (chrome_browser->processes[index].process_type ==
        content::PROCESS_TYPE_UNKNOWN) {
      chrome_browser->processes.erase(
          chrome_browser->processes.begin() + index);
      index--;
    }
  }

  if (user_metrics_mode_ == UPDATE_USER_METRICS)
    UpdateHistograms();

  OnDetailsAvailable();
}

void MemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.
  // Memory is measured in KB.

  const ProcessData& browser = *ChromeBrowser();
  size_t aggregate_memory = 0;
  int chrome_count = 0;
  int extension_count = 0;
  int plugin_count = 0;
  int pepper_plugin_count = 0;
  int pepper_plugin_broker_count = 0;
  int renderer_count = 0;
  int other_count = 0;
  int worker_count = 0;
  int process_limit = content::RenderProcessHost::GetMaxRendererProcessCount();
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.priv);
    aggregate_memory += sample;
    switch (browser.processes[index].process_type) {
      case content::PROCESS_TYPE_BROWSER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Browser", sample);
        continue;
      case content::PROCESS_TYPE_RENDERER: {
        ProcessMemoryInformation::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ProcessMemoryInformation::RENDERER_EXTENSION:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Extension", sample);
            extension_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Chrome", sample);
            chrome_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            continue;
          case ProcessMemoryInformation::RENDERER_NORMAL:
          default:
            // TODO(erikkay): Should we bother splitting out the other subtypes?
            UMA_HISTOGRAM_MEMORY_KB("Memory.Renderer", sample);
            int diff;
            if (memory_growth_tracker_ &&
                memory_growth_tracker_->UpdateSample(
                    browser.processes[index].pid, sample, &diff)) {
              if (diff < 0)
                UMA_HISTOGRAM_MEMORY_KB("Memory.RendererShrinkIn30Min", -diff);
              else
                UMA_HISTOGRAM_MEMORY_KB("Memory.RendererGrowthIn30Min", diff);
            }
            renderer_count++;
            continue;
        }
      }
      case content::PROCESS_TYPE_PLUGIN:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Plugin", sample);
        plugin_count++;
        continue;
      case content::PROCESS_TYPE_WORKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Worker", sample);
        worker_count++;
        continue;
      case content::PROCESS_TYPE_UTILITY:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Utility", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_ZYGOTE:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Zygote", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_SANDBOX_HELPER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.SandboxHelper", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_GPU:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Gpu", sample);
        other_count++;
        continue;
      case content::PROCESS_TYPE_PPAPI_PLUGIN:
        UMA_HISTOGRAM_MEMORY_KB("Memory.PepperPlugin", sample);
        pepper_plugin_count++;
        continue;
      case content::PROCESS_TYPE_PPAPI_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.PepperPluginBroker", sample);
        pepper_plugin_broker_count++;
        continue;
      case PROCESS_TYPE_NACL_LOADER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClient", sample);
        other_count++;
        continue;
      case PROCESS_TYPE_NACL_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClientBroker", sample);
        other_count++;
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

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessLimit", process_limit);
  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
      static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.ChromeProcessCount", chrome_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.ExtensionProcessCount", extension_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.OtherProcessCount", other_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PluginProcessCount", plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PepperPluginProcessCount",
      pepper_plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PepperPluginBrokerProcessCount",
      pepper_plugin_broker_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.RendererProcessCount", renderer_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.WorkerProcessCount", worker_count);
  // TODO(viettrungluu): Do we want separate counts for the other
  // (platform-specific) process types?

  int total_sample = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Total", total_sample);

#if defined(OS_CHROMEOS)
  UpdateSwapHistograms();
#endif
}

#if defined(OS_CHROMEOS)
void MemoryDetails::UpdateSwapHistograms() {
  UMA_HISTOGRAM_BOOLEAN("Memory.Swap.HaveSwapped", swap_info_.num_writes > 0);
  if (swap_info_.num_writes == 0)
    return;

  // Only record swap info when any swaps have happened, to give us more
  // detail in the histograms.
  const ProcessData& browser = *ChromeBrowser();
  size_t aggregate_memory = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.swapped);
    aggregate_memory += sample;
    switch (browser.processes[index].process_type) {
      case content::PROCESS_TYPE_BROWSER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Browser", sample);
        continue;
      case content::PROCESS_TYPE_RENDERER: {
        ProcessMemoryInformation::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ProcessMemoryInformation::RENDERER_EXTENSION:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Extension", sample);
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Chrome", sample);
            continue;
          case ProcessMemoryInformation::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            continue;
          case ProcessMemoryInformation::RENDERER_NORMAL:
          default:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Renderer", sample);
            continue;
        }
      }
      case content::PROCESS_TYPE_PLUGIN:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Plugin", sample);
        continue;
      case content::PROCESS_TYPE_WORKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Worker", sample);
        continue;
      case content::PROCESS_TYPE_UTILITY:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Utility", sample);
        continue;
      case content::PROCESS_TYPE_ZYGOTE:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Zygote", sample);
        continue;
      case content::PROCESS_TYPE_SANDBOX_HELPER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.SandboxHelper", sample);
        continue;
      case content::PROCESS_TYPE_GPU:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.Gpu", sample);
        continue;
      case content::PROCESS_TYPE_PPAPI_PLUGIN:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.PepperPlugin", sample);
        continue;
      case content::PROCESS_TYPE_PPAPI_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.PepperPluginBroker", sample);
        continue;
      case PROCESS_TYPE_NACL_LOADER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.NativeClient", sample);
        continue;
      case PROCESS_TYPE_NACL_BROKER:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Swap.NativeClientBroker", sample);
        continue;
      default:
        NOTREACHED();
        continue;
    }
  }

  int total_sample = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Swap.Total", total_sample);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.CompressedDataSize",
                              swap_info_.compr_data_size / (1024 * 1024),
                              1, 4096, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.OriginalDataSize",
                              swap_info_.orig_data_size / (1024 * 1024),
                              1, 4096, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.MemUsedTotal",
                              swap_info_.mem_used_total / (1024 * 1024),
                              1, 4096, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.NumReads",
                              swap_info_.num_reads,
                              1, 100000000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Swap.NumWrites",
                              swap_info_.num_writes,
                              1, 100000000, 100);

  if (swap_info_.orig_data_size > 0 && swap_info_.compr_data_size > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Memory.Swap.CompressionRatio",
        swap_info_.orig_data_size / swap_info_.compr_data_size,
        1, 20, 20);
  }
}

#endif
