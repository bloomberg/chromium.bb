// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"

// TODO(thestig): Remove ifdefs when extensions no longer build on mobile.
#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/renderer_host/chrome_extension_message_filter.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "extensions/browser/extension_message_filter.h"
#endif

using content::BrowserThread;
using content::BrowserURLHandler;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using content::WebPreferences;

namespace extensions {

ChromeContentBrowserClientExtensionsPart::
    ChromeContentBrowserClientExtensionsPart() {
}

ChromeContentBrowserClientExtensionsPart::
    ~ChromeContentBrowserClientExtensionsPart() {
}

void ChromeContentBrowserClientExtensionsPart::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
#if defined(ENABLE_EXTENSIONS)
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->AddFilter(new ChromeExtensionMessageFilter(id, profile));
  host->AddFilter(new ExtensionMessageFilter(id, profile));
  SendExtensionWebRequestStatusToHost(host);
#endif
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  Profile* profile = Profile::FromBrowserContext(
      site_instance->GetProcess()->GetBrowserContext());
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const Extension* extension = service->extensions()->GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
  if (!extension)
    return;

  ProcessMap::Get(profile)->Insert(extension->id(),
                                   site_instance->GetProcess()->GetID(),
                                   site_instance->GetId());

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&InfoMap::RegisterExtensionProcess,
                                     ExtensionSystem::Get(profile)->info_map(),
                                     extension->id(),
                                     site_instance->GetProcess()->GetID(),
                                     site_instance->GetId()));
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceDeleting(
    SiteInstance* site_instance) {
  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const Extension* extension = service->extensions()->GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
  if (!extension)
    return;

  ProcessMap::Get(profile)->Remove(extension->id(),
                                   site_instance->GetProcess()->GetID(),
                                   site_instance->GetId());

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&InfoMap::UnregisterExtensionProcess,
                                     ExtensionSystem::Get(profile)->info_map(),
                                     extension->id(),
                                     site_instance->GetProcess()->GetID(),
                                     site_instance->GetId()));
}

void ChromeContentBrowserClientExtensionsPart::WorkerProcessCreated(
    SiteInstance* site_instance,
    int worker_process_id) {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!extension_registry)
    return;
  const Extension* extension =
      extension_registry->enabled_extensions().GetExtensionOrAppByURL(
          site_instance->GetSiteURL());
  if (!extension)
    return;
  ExtensionSystem* extension_system =
      ExtensionSystem::Get(site_instance->GetBrowserContext());
  extension_system->info_map()->RegisterExtensionWorkerProcess(
      extension->id(), worker_process_id, site_instance->GetId());
}

void ChromeContentBrowserClientExtensionsPart::WorkerProcessTerminated(
    SiteInstance* site_instance,
    int worker_process_id) {
  ExtensionSystem* extension_system =
      ExtensionSystem::Get(site_instance->GetBrowserContext());
  extension_system->info_map()->UnregisterExtensionWorkerProcess(
      worker_process_id);
}

void ChromeContentBrowserClientExtensionsPart::OverrideWebkitPrefs(
    RenderViewHost* rvh,
    const GURL& url,
    WebPreferences* web_prefs) {
  Profile* profile =
      Profile::FromBrowserContext(rvh->GetProcess()->GetBrowserContext());

  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  // Note: it's not possible for kExtensionsScheme to change during the lifetime
  // of the process.
  //
  // Ensure that we are only granting extension preferences to URLs with
  // the correct scheme. Without this check, chrome-guest:// schemes used by
  // webview tags as well as hosts that happen to match the id of an
  // installed extension would get the wrong preferences.
  const GURL& site_url = rvh->GetSiteInstance()->GetSiteURL();
  if (!site_url.SchemeIs(kExtensionScheme))
    return;

  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  ViewType view_type = GetViewType(web_contents);
  const Extension* extension = service->extensions()->GetByID(site_url.host());
  extension_webkit_preferences::SetPreferences(extension, view_type, web_prefs);
}

void ChromeContentBrowserClientExtensionsPart::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  handler->AddHandlerPair(&ExtensionWebUI::HandleChromeURLOverride,
                          BrowserURLHandler::null_handler());
  handler->AddHandlerPair(BrowserURLHandler::null_handler(),
                          &ExtensionWebUI::HandleChromeURLOverrideReverse);
}

void ChromeContentBrowserClientExtensionsPart::
    GetAdditionalAllowedSchemesForFileSystem(
        std::vector<std::string>* additional_allowed_schemes) {
  additional_allowed_schemes->push_back(kExtensionScheme);
}

void ChromeContentBrowserClientExtensionsPart::GetURLRequestAutoMountHandlers(
    std::vector<fileapi::URLRequestAutoMountHandler>* handlers) {
#if defined(ENABLE_EXTENSIONS)
  handlers->push_back(
      base::Bind(MediaFileSystemBackend::AttemptAutoMountForURLRequest));
#endif
}

void ChromeContentBrowserClientExtensionsPart::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    ScopedVector<fileapi::FileSystemBackend>* additional_backends) {
#if defined(ENABLE_EXTENSIONS)
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  additional_backends->push_back(new MediaFileSystemBackend(
      storage_partition_path,
      pool->GetSequencedTaskRunner(
                pool->GetNamedSequenceToken(
                    MediaFileSystemBackend::kMediaTaskRunnerName)).get()));

  additional_backends->push_back(new sync_file_system::SyncFileSystemBackend(
      Profile::FromBrowserContext(browser_context)));
#endif
}

void ChromeContentBrowserClientExtensionsPart::
    AppendExtraRendererCommandLineSwitches(base::CommandLine* command_line,
                                           content::RenderProcessHost* process,
                                           Profile* profile) {
  if (!process)
    return;
  DCHECK(profile);
  if (ProcessMap::Get(profile)->Contains(process->GetID()))
    command_line->AppendSwitch(switches::kExtensionProcess);
}

}  // namespace extensions
