// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include "app/l10n_util.h"
#include "base/file_version_info.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"

#if defined(OS_LINUX)
#include "chrome/browser/zygote_host_linux.h"
#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#endif

// About threading:
//
// This operation will hit no fewer than 3 threads.
//
// The ChildProcessInfo::Iterator can only be accessed from the IO thread.
//
// The RenderProcessHostIterator can only be accessed from the UI thread.
//
// This operation can take 30-100ms to complete.  We never want to have
// one task run for that long on the UI or IO threads.  So, we run the
// expensive parts of this operation over on the file thread.
//

void MemoryDetails::StartFetch() {
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // In order to process this request, we need to use the plugin information.
  // However, plugin process information is only available from the IO thread.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectChildInfoOnIOThread));
}

void MemoryDetails::CollectChildInfoOnIOThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  std::vector<ProcessMemoryInformation> child_info;

  // Collect the list of child processes.
  for (ChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
    ProcessMemoryInformation info;
    info.pid = base::GetProcId(iter->handle());
    if (!info.pid)
      continue;

    info.type = iter->type();
    info.titles.push_back(iter->name());
    child_info.push_back(info);
  }

  // Now go do expensive memory lookups from the file thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectProcessData, child_info));
}

void MemoryDetails::CollectChildInfoOnUIThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

#if defined(OS_LINUX)
  const pid_t zygote_pid = Singleton<ZygoteHost>()->pid();
  const pid_t sandbox_helper_pid = Singleton<RenderSandboxHostLinux>()->pid();
#endif

  ProcessData* const chrome_browser = ChromeBrowser();
  // Get more information about the process.
  for (size_t index = 0; index < chrome_browser->processes.size();
      index++) {
    // Check if it's a renderer, if so get the list of page titles in it and
    // check if it's a diagnostics-related process.  We skip all diagnostics
    // pages (e.g. "about:xxx" URLs).  Iterate the RenderProcessHosts to find
    // the tab contents.
    ProcessMemoryInformation& process =
        chrome_browser->processes[index];

    for (RenderProcessHost::iterator renderer_iter(
         RenderProcessHost::AllHostsIterator()); !renderer_iter.IsAtEnd();
         renderer_iter.Advance()) {
      DCHECK(renderer_iter.GetCurrentValue());
      if (process.pid != renderer_iter.GetCurrentValue()->process().pid())
        continue;
      process.type = ChildProcessInfo::RENDER_PROCESS;
      // The RenderProcessHost may host multiple TabContents.  Any
      // of them which contain diagnostics information make the whole
      // process be considered a diagnostics process.
      //
      // NOTE: This is a bit dangerous.  We know that for now, listeners
      //       are always RenderWidgetHosts.  But in theory, they don't
      //       have to be.
      RenderProcessHost::listeners_iterator iter(
          renderer_iter.GetCurrentValue()->ListenersIterator());
      for (; !iter.IsAtEnd(); iter.Advance()) {
        const RenderWidgetHost* widget =
            static_cast<const RenderWidgetHost*>(iter.GetCurrentValue());
        DCHECK(widget);
        if (!widget || !widget->IsRenderView())
          continue;

        const RenderViewHost* host = static_cast<const RenderViewHost*>(widget);
        TabContents* contents = NULL;
        if (host->delegate())
          contents = host->delegate()->GetAsTabContents();
        if (!contents)
          continue;
        std::wstring title = UTF16ToWideHack(contents->GetTitle());
        if (!title.length())
          title = L"Untitled";
        process.titles.push_back(title);

        // We need to check the pending entry as well as the virtual_url to
        // see if it's an about:memory URL (we don't want to count these in the
        // total memory usage of the browser).
        //
        // When we reach here, about:memory will be the pending entry since we
        // haven't responded with any data such that it would be committed. If
        // you have another about:memory tab open (which would be committed),
        // we don't want to count it either, so we also check the last committed
        // entry.
        //
        // Either the pending or last committed entries can be NULL.
        const NavigationEntry* pending_entry =
            contents->controller().pending_entry();
        const NavigationEntry* last_committed_entry =
            contents->controller().GetLastCommittedEntry();
        if ((last_committed_entry &&
             LowerCaseEqualsASCII(last_committed_entry->virtual_url().spec(),
                                  chrome::kAboutMemoryURL)) ||
            (pending_entry &&
             LowerCaseEqualsASCII(pending_entry->virtual_url().spec(),
                                  chrome::kAboutMemoryURL)))
          process.is_diagnostics = true;
      }
    }

#if defined(OS_LINUX)
    if (process.pid == zygote_pid) {
      process.type = ChildProcessInfo::ZYGOTE_PROCESS;
    } else if (process.pid == sandbox_helper_pid) {
      process.type = ChildProcessInfo::SANDBOX_HELPER_PROCESS;
    }
#endif
  }

  // Get rid of other Chrome processes that are from a different profile.
  for (size_t index = 0; index < chrome_browser->processes.size();
      index++) {
    if (chrome_browser->processes[index].type ==
        ChildProcessInfo::UNKNOWN_PROCESS) {
      chrome_browser->processes.erase(
          chrome_browser->processes.begin() + index);
      index--;
    }
  }

  UpdateHistograms();

  OnDetailsAvailable();
}

void MemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.
  // Memory is measured in KB.

  const ProcessData& browser = *ChromeBrowser();
  size_t aggregate_memory = 0;
  int plugin_count = 0;
  int worker_count = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.priv);
    aggregate_memory += sample;
    switch (browser.processes[index].type) {
      case ChildProcessInfo::BROWSER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Browser", sample);
        break;
      case ChildProcessInfo::RENDER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Renderer", sample);
        break;
      case ChildProcessInfo::PLUGIN_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Plugin", sample);
        plugin_count++;
        break;
      case ChildProcessInfo::WORKER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Worker", sample);
        worker_count++;
        break;
      case ChildProcessInfo::UTILITY_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Utility", sample);
        break;
      case ChildProcessInfo::ZYGOTE_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Zygote", sample);
        break;
      case ChildProcessInfo::SANDBOX_HELPER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.SandboxHelper", sample);
        break;
      case ChildProcessInfo::NACL_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClient", sample);
        break;
      default:
        NOTREACHED();
    }
  }
  UMA_HISTOGRAM_MEMORY_KB("Memory.BackingStore",
                          BackingStoreManager::MemorySize() / 1024);

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
      static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.PluginProcessCount", plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.WorkerProcessCount", worker_count);
  // TODO(viettrungluu): Do we want separate counts for the other
  // (platform-specific) process types?

  int total_sample = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Total", total_sample);
}
