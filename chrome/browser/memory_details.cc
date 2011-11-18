// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_view_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/renderer_host/backing_store_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bindings_policy.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host_linux.h"
#endif

using content::BrowserThread;

ProcessMemoryInformation::ProcessMemoryInformation()
    : pid(0),
      num_processes(0),
      is_diagnostics(false),
      type(ChildProcessInfo::UNKNOWN_PROCESS),
      renderer_type(ChildProcessInfo::RENDERER_UNKNOWN) {
}

ProcessMemoryInformation::~ProcessMemoryInformation() {}

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
  // This might get called from the UI or FILE threads, but should not be
  // getting called from the IO thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  // In order to process this request, we need to use the plugin information.
  // However, plugin process information is only available from the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnIOThread, this));
}

MemoryDetails::~MemoryDetails() {}

void MemoryDetails::CollectChildInfoOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<ProcessMemoryInformation> child_info;

  // Collect the list of child processes.
  for (BrowserChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
    ProcessMemoryInformation info;
    info.pid = base::GetProcId(iter->handle());
    if (!info.pid)
      continue;

    info.type = iter->type();
    info.renderer_type = iter->renderer_type();
    info.titles.push_back(iter->name());
    child_info.push_back(info);
  }

  // Now go do expensive memory lookups from the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MemoryDetails::CollectProcessData, this, child_info));
}

void MemoryDetails::CollectChildInfoOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  const pid_t zygote_pid = ZygoteHost::GetInstance()->pid();
  const pid_t sandbox_helper_pid = RenderSandboxHostLinux::GetInstance()->pid();
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

    for (content::RenderProcessHost::iterator renderer_iter(
            content::RenderProcessHost::AllHostsIterator());
         !renderer_iter.IsAtEnd(); renderer_iter.Advance()) {
      content::RenderProcessHost* render_process_host =
          renderer_iter.GetCurrentValue();
      DCHECK(render_process_host);
      // Ignore processes that don't have a connection, such as crashed tabs.
      if (!render_process_host->HasConnection() ||
          process.pid != base::GetProcId(render_process_host->GetHandle())) {
        continue;
      }
      process.type = ChildProcessInfo::RENDER_PROCESS;
      Profile* profile =
          Profile::FromBrowserContext(
              render_process_host->GetBrowserContext());
      ExtensionService* extension_service = profile->GetExtensionService();
      extensions::ProcessMap* extension_process_map =
          extension_service->process_map();

      // The RenderProcessHost may host multiple TabContents.  Any
      // of them which contain diagnostics information make the whole
      // process be considered a diagnostics process.
      //
      // NOTE: This is a bit dangerous.  We know that for now, listeners
      //       are always RenderWidgetHosts.  But in theory, they don't
      //       have to be.
      content::RenderProcessHost::listeners_iterator iter(
          render_process_host->ListenersIterator());
      for (; !iter.IsAtEnd(); iter.Advance()) {
        const RenderWidgetHost* widget =
            static_cast<const RenderWidgetHost*>(iter.GetCurrentValue());
        DCHECK(widget);
        if (!widget || !widget->IsRenderView())
          continue;

        const RenderViewHost* host = static_cast<const RenderViewHost*>(widget);
        RenderViewHostDelegate* host_delegate = host->delegate();
        DCHECK(host_delegate);
        GURL url = host_delegate->GetURL();
        content::ViewType type = host_delegate->GetRenderViewType();
        if (host->enabled_bindings() & content::BINDINGS_POLICY_WEB_UI) {
          // TODO(erikkay) the type for devtools doesn't actually appear to
          // be set.
          if (type == content::VIEW_TYPE_DEV_TOOLS_UI)
            process.renderer_type = ChildProcessInfo::RENDERER_DEVTOOLS;
          else
            process.renderer_type = ChildProcessInfo::RENDERER_CHROME;
        } else if (extension_process_map->Contains(host->process()->GetID())) {
          // For our purposes, don't count processes containing only hosted apps
          // as extension processes. See also: crbug.com/102533.
          std::set<std::string> extension_ids =
              extension_process_map->GetExtensionsInProcess(
                  host->process()->GetID());
          for (std::set<std::string>::iterator iter = extension_ids.begin();
               iter != extension_ids.end(); ++iter) {
            const Extension* extension =
                extension_service->GetExtensionById(*iter, false);
            if (extension && !extension->is_hosted_app()) {
              process.renderer_type = ChildProcessInfo::RENDERER_EXTENSION;
              break;
            }
          }
        }
        TabContents* contents = host_delegate->GetAsTabContents();
        if (!contents) {
          if (extension_process_map->Contains(host->process()->GetID())) {
            const Extension* extension =
                extension_service->GetExtensionByURL(url);
            if (extension) {
              string16 title = UTF8ToUTF16(extension->name());
              process.titles.push_back(title);
            }
          } else if (process.renderer_type ==
                     ChildProcessInfo::RENDERER_UNKNOWN) {
            process.titles.push_back(UTF8ToUTF16(url.spec()));
            switch (type) {
              case chrome::VIEW_TYPE_BACKGROUND_CONTENTS:
                process.renderer_type =
                    ChildProcessInfo::RENDERER_BACKGROUND_APP;
                break;
              case content::VIEW_TYPE_INTERSTITIAL_PAGE:
                process.renderer_type = ChildProcessInfo::RENDERER_INTERSTITIAL;
                break;
              case chrome::VIEW_TYPE_NOTIFICATION:
                process.renderer_type = ChildProcessInfo::RENDERER_NOTIFICATION;
                break;
              default:
                process.renderer_type = ChildProcessInfo::RENDERER_UNKNOWN;
                break;
            }
          }
          continue;
        }

        // Since We have a TabContents and and the renderer type hasn't been
        // set yet, it must be a normal tabbed renderer.
        if (process.renderer_type == ChildProcessInfo::RENDERER_UNKNOWN)
          process.renderer_type = ChildProcessInfo::RENDERER_NORMAL;

        string16 title = contents->GetTitle();
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
            contents->controller().pending_entry();
        const NavigationEntry* last_committed_entry =
            contents->controller().GetLastCommittedEntry();
        if ((last_committed_entry &&
             LowerCaseEqualsASCII(last_committed_entry->virtual_url().spec(),
                                  chrome::kChromeUIMemoryURL)) ||
            (pending_entry &&
             LowerCaseEqualsASCII(pending_entry->virtual_url().spec(),
                                  chrome::kChromeUIMemoryURL)))
          process.is_diagnostics = true;
      }
    }

#if defined(OS_POSIX) && !defined(OS_MACOSX)
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
  int chrome_count = 0;
  int extension_count = 0;
  int plugin_count = 0;
  int pepper_plugin_count = 0;
  int renderer_count = 0;
  int other_count = 0;
  int worker_count = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.priv);
    aggregate_memory += sample;
    switch (browser.processes[index].type) {
      case ChildProcessInfo::BROWSER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Browser", sample);
        break;
      case ChildProcessInfo::RENDER_PROCESS: {
        ChildProcessInfo::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ChildProcessInfo::RENDERER_EXTENSION:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Extension", sample);
            extension_count++;
            break;
          case ChildProcessInfo::RENDERER_CHROME:
            UMA_HISTOGRAM_MEMORY_KB("Memory.Chrome", sample);
            chrome_count++;
            break;
          case ChildProcessInfo::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            break;
          case ChildProcessInfo::RENDERER_NORMAL:
          default:
            // TODO(erikkay): Should we bother splitting out the other subtypes?
            UMA_HISTOGRAM_MEMORY_KB("Memory.Renderer", sample);
            renderer_count++;
            break;
        }
        break;
      }
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
        other_count++;
        break;
      case ChildProcessInfo::ZYGOTE_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Zygote", sample);
        other_count++;
        break;
      case ChildProcessInfo::SANDBOX_HELPER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.SandboxHelper", sample);
        other_count++;
        break;
      case ChildProcessInfo::NACL_LOADER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClient", sample);
        other_count++;
        break;
      case ChildProcessInfo::NACL_BROKER_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.NativeClientBroker", sample);
        other_count++;
        break;
      case ChildProcessInfo::GPU_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.Gpu", sample);
        other_count++;
        break;
      case ChildProcessInfo::PPAPI_PLUGIN_PROCESS:
        UMA_HISTOGRAM_MEMORY_KB("Memory.PepperPlugin", sample);
        pepper_plugin_count++;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  UMA_HISTOGRAM_MEMORY_KB("Memory.BackingStore",
                          BackingStoreManager::MemorySize() / 1024);

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
      static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.ChromeProcessCount", chrome_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.ExtensionProcessCount", extension_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.OtherProcessCount", other_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PluginProcessCount", plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.PepperPluginProcessCount",
      pepper_plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.RendererProcessCount", renderer_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.WorkerProcessCount", worker_count);
  // TODO(viettrungluu): Do we want separate counts for the other
  // (platform-specific) process types?

  int total_sample = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Total", total_sample);
}
