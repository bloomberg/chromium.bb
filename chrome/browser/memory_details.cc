// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
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
#include "content/public/common/content_constants.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "content/public/browser/zygote_host_linux.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
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
void MemoryDetails::StartFetch(CollectionMode mode) {
  // This might get called from the UI or FILE threads, but should not be
  // getting called from the IO thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  // In order to process this request, we need to use the plugin information.
  // However, plugin process information is only available from the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnIOThread, this, mode));
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

void MemoryDetails::CollectChildInfoOnIOThread(CollectionMode mode) {
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
      base::Bind(&MemoryDetails::CollectProcessData, this, mode, child_info));
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
      extensions::ExtensionRegistry* extension_registry =
          extensions::ExtensionRegistry::Get(context);
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
              extension_registry->enabled_extensions().GetByID(*iter);
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
            extension_registry->enabled_extensions().GetByID(url.host());
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

  OnDetailsAvailable();
}
