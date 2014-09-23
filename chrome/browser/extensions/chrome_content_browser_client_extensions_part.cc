// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include <set>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/browser_permissions_policy_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_extension_message_filter.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/switches.h"

using content::BrowserThread;
using content::BrowserURLHandler;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using content::WebPreferences;

namespace extensions {

namespace {

// Used by the GetPrivilegeRequiredByUrl() and GetProcessPrivilege() functions
// below.  Extension, and isolated apps require different privileges to be
// granted to their RenderProcessHosts.  This classification allows us to make
// sure URLs are served by hosts with the right set of privileges.
enum RenderProcessHostPrivilege {
  PRIV_NORMAL,
  PRIV_HOSTED,
  PRIV_ISOLATED,
  PRIV_EXTENSION,
};

RenderProcessHostPrivilege GetPrivilegeRequiredByUrl(
    const GURL& url,
    ExtensionService* service) {
  // Default to a normal renderer cause it is lower privileged. This should only
  // occur if the URL on a site instance is either malformed, or uninitialized.
  // If it is malformed, then there is no need for better privileges anyways.
  // If it is uninitialized, but eventually settles on being an a scheme other
  // than normal webrenderer, the navigation logic will correct us out of band
  // anyways.
  if (!url.is_valid())
    return PRIV_NORMAL;

  if (!url.SchemeIs(kExtensionScheme))
    return PRIV_NORMAL;

  const Extension* extension = service->extensions()->GetByID(url.host());
  if (extension && AppIsolationInfo::HasIsolatedStorage(extension))
    return PRIV_ISOLATED;
  if (extension && extension->is_hosted_app())
    return PRIV_HOSTED;
  return PRIV_EXTENSION;
}

RenderProcessHostPrivilege GetProcessPrivilege(
    content::RenderProcessHost* process_host,
    ProcessMap* process_map,
    ExtensionService* service) {
  std::set<std::string> extension_ids =
      process_map->GetExtensionsInProcess(process_host->GetID());
  if (extension_ids.empty())
    return PRIV_NORMAL;

  for (std::set<std::string>::iterator iter = extension_ids.begin();
       iter != extension_ids.end(); ++iter) {
    const Extension* extension = service->GetExtensionById(*iter, false);
    if (extension && AppIsolationInfo::HasIsolatedStorage(extension))
      return PRIV_ISOLATED;
    if (extension && extension->is_hosted_app())
      return PRIV_HOSTED;
  }

  return PRIV_EXTENSION;
}

}  // namespace

ChromeContentBrowserClientExtensionsPart::
    ChromeContentBrowserClientExtensionsPart() {
  permissions_policy_delegate_.reset(new BrowserPermissionsPolicyDelegate());
}

ChromeContentBrowserClientExtensionsPart::
    ~ChromeContentBrowserClientExtensionsPart() {
}

// static
GURL ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(
    Profile* profile, const GURL& url) {
  // If the input |url| is part of an installed app, the effective URL is an
  // extension URL with the ID of that extension as the host. This has the
  // effect of grouping apps together in a common SiteInstance.
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return url;

  const Extension* extension =
      extension_service->extensions()->GetHostedAppByURL(url);
  if (!extension)
    return url;

  // Bookmark apps do not use the hosted app process model, and should be
  // treated as normal URLs.
  if (extension->from_bookmark())
    return url;

  // If the URL is part of an extension's web extent, convert it to an
  // extension URL.
  return extension->GetResourceURL(url.path());
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldUseProcessPerSite(
    Profile* profile, const GURL& effective_url) {
  if (!effective_url.SchemeIs(kExtensionScheme))
    return false;

  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return false;

  const Extension* extension =
      extension_service->extensions()->GetExtensionOrAppByURL(effective_url);
  if (!extension)
    return false;

  // If the URL is part of a hosted app that does not have the background
  // permission, or that does not allow JavaScript access to the background
  // page, we want to give each instance its own process to improve
  // responsiveness.
  if (extension->GetType() == Manifest::TYPE_HOSTED_APP) {
    if (!extension->permissions_data()->HasAPIPermission(
            APIPermission::kBackground) ||
        !BackgroundInfo::AllowJSAccess(extension)) {
      return false;
    }
  }

  // Hosted apps that have script access to their background page must use
  // process per site, since all instances can make synchronous calls to the
  // background window.  Other extensions should use process per site as well.
  return true;
}

// static
bool ChromeContentBrowserClientExtensionsPart::CanCommitURL(
    content::RenderProcessHost* process_host, const GURL& url) {
  // We need to let most extension URLs commit in any process, since this can
  // be allowed due to web_accessible_resources.  Most hosted app URLs may also
  // load in any process (e.g., in an iframe).  However, the Chrome Web Store
  // cannot be loaded in iframes and should never be requested outside its
  // process.
  Profile* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return true;

  const Extension* new_extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  if (new_extension &&
      new_extension->is_hosted_app() &&
      new_extension->id() == extensions::kWebStoreAppId &&
      !ProcessMap::Get(profile)->Contains(
          new_extension->id(), process_host->GetID())) {
    return false;
  }
  return true;
}

// static
bool ChromeContentBrowserClientExtensionsPart::IsSuitableHost(
    Profile* profile,
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  DCHECK(profile);

  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  ProcessMap* process_map = ProcessMap::Get(profile);

  // These may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!service || !process_map)
    return true;

  // Otherwise, just make sure the process privilege matches the privilege
  // required by the site.
  RenderProcessHostPrivilege privilege_required =
      GetPrivilegeRequiredByUrl(site_url, service);
  return GetProcessPrivilege(process_host, process_map, service) ==
      privilege_required;
}

// static
bool
ChromeContentBrowserClientExtensionsPart::ShouldTryToUseExistingProcessHost(
    Profile* profile, const GURL& url) {
  // This function is trying to limit the amount of processes used by extensions
  // with background pages. It uses a globally set percentage of processes to
  // run such extensions and if the limit is exceeded, it returns true, to
  // indicate to the content module to group extensions together.
  ExtensionService* service = profile ?
      ExtensionSystem::Get(profile)->extension_service() : NULL;
  if (!service)
    return false;

  // We have to have a valid extension with background page to proceed.
  const Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  if (!extension)
    return false;
  if (!BackgroundInfo::HasBackgroundPage(extension))
    return false;

  std::set<int> process_ids;
  size_t max_process_count =
      content::RenderProcessHost::GetMaxRendererProcessCount();

  // Go through all profiles to ensure we have total count of extension
  // processes containing background pages, otherwise one profile can
  // starve the other.
  std::vector<Profile*> profiles = g_browser_process->profile_manager()->
      GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    ProcessManager* epm = ExtensionSystem::Get(profiles[i])->process_manager();
    for (ProcessManager::const_iterator iter = epm->background_hosts().begin();
         iter != epm->background_hosts().end(); ++iter) {
      const ExtensionHost* host = *iter;
      process_ids.insert(host->render_process_host()->GetID());
    }
  }

  return (process_ids.size() >
          (max_process_count * chrome::kMaxShareOfExtensionProcesses));
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldSwapBrowsingInstancesForNavigation(SiteInstance* site_instance,
                                             const GURL& current_url,
                                             const GURL& new_url) {
  // If we don't have an ExtensionService, then rely on the SiteInstance logic
  // in RenderFrameHostManager to decide when to swap.
  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;

  // We must use a new BrowsingInstance (forcing a process swap and disabling
  // scripting by existing tabs) if one of the URLs is an extension and the
  // other is not the exact same extension.
  //
  // We ignore hosted apps here so that other tabs in their BrowsingInstance can
  // use postMessage with them.  (The exception is the Chrome Web Store, which
  // is a hosted app that requires its own BrowsingInstance.)  Navigations
  // to/from a hosted app will still trigger a SiteInstance swap in
  // RenderFrameHostManager.
  const Extension* current_extension =
      service->extensions()->GetExtensionOrAppByURL(current_url);
  if (current_extension &&
      current_extension->is_hosted_app() &&
      current_extension->id() != extensions::kWebStoreAppId)
    current_extension = NULL;

  const Extension* new_extension =
      service->extensions()->GetExtensionOrAppByURL(new_url);
  if (new_extension &&
      new_extension->is_hosted_app() &&
      new_extension->id() != extensions::kWebStoreAppId)
    new_extension = NULL;

  // First do a process check.  We should force a BrowsingInstance swap if the
  // current process doesn't know about new_extension, even if current_extension
  // is somehow the same as new_extension.
  ProcessMap* process_map = ProcessMap::Get(profile);
  if (new_extension &&
      site_instance->HasProcess() &&
      !process_map->Contains(
          new_extension->id(), site_instance->GetProcess()->GetID()))
    return true;

  // Otherwise, swap BrowsingInstances if current_extension and new_extension
  // differ.
  return current_extension != new_extension;
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldSwapProcessesForRedirect(
    content::ResourceContext* resource_context,
    const GURL& current_url,
    const GURL& new_url) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  return CrossesExtensionProcessBoundary(
      io_data->GetExtensionInfoMap()->extensions(),
      current_url, new_url, false);
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldAllowOpenURL(
    content::SiteInstance* site_instance,
    const GURL& from_url,
    const GURL& to_url,
    bool* result) {
  DCHECK(result);

  // Do not allow pages from the web or other extensions navigate to
  // non-web-accessible extension resources.
  if (to_url.SchemeIs(kExtensionScheme) &&
      (from_url.SchemeIsHTTPOrHTTPS() || from_url.SchemeIs(kExtensionScheme))) {
    Profile* profile = Profile::FromBrowserContext(
        site_instance->GetProcess()->GetBrowserContext());
    ExtensionService* service =
        ExtensionSystem::Get(profile)->extension_service();
    if (!service) {
      *result = true;
      return true;
    }
    const Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(to_url);
    if (!extension) {
      *result = true;
      return true;
    }
    const Extension* from_extension =
        service->extensions()->GetExtensionOrAppByURL(
            site_instance->GetSiteURL());
    if (from_extension && from_extension->id() == extension->id()) {
      *result = true;
      return true;
    }

    if (!WebAccessibleResourcesInfo::IsResourceWebAccessible(
            extension, to_url.path())) {
      *result = false;
      return true;
    }
  }
  return false;
}

// static
void ChromeContentBrowserClientExtensionsPart::SetSigninProcess(
    content::SiteInstance* site_instance) {
  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  DCHECK(profile);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::SetSigninProcess,
                 ExtensionSystem::Get(profile)->info_map(),
                 site_instance->GetProcess()->GetID()));
}

void ChromeContentBrowserClientExtensionsPart::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->AddFilter(new ChromeExtensionMessageFilter(id, profile));
  host->AddFilter(new ExtensionMessageFilter(id, profile));
  extension_web_request_api_helpers::SendExtensionWebRequestStatusToHost(host);
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
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
  handlers->push_back(
      base::Bind(MediaFileSystemBackend::AttemptAutoMountForURLRequest));
}

void ChromeContentBrowserClientExtensionsPart::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    ScopedVector<storage::FileSystemBackend>* additional_backends) {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  additional_backends->push_back(new MediaFileSystemBackend(
      storage_partition_path,
      pool->GetSequencedTaskRunner(
                pool->GetNamedSequenceToken(
                    MediaFileSystemBackend::kMediaTaskRunnerName)).get()));

  additional_backends->push_back(new sync_file_system::SyncFileSystemBackend(
      Profile::FromBrowserContext(browser_context)));
}

void ChromeContentBrowserClientExtensionsPart::
    AppendExtraRendererCommandLineSwitches(base::CommandLine* command_line,
                                           content::RenderProcessHost* process,
                                           Profile* profile) {
  if (!process)
    return;
  DCHECK(profile);
  if (ProcessMap::Get(profile)->Contains(process->GetID())) {
    command_line->AppendSwitch(switches::kExtensionProcess);
#if defined(ENABLE_WEBRTC)
    command_line->AppendSwitch(::switches::kEnableWebRtcHWH264Encoding);
#endif
  }
}

}  // namespace extensions
