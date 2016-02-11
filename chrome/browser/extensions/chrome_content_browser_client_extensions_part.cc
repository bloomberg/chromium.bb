// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include <stddef.h>

#include <set>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "components/guest_view/browser/guest_view_message_filter.h"
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
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

using content::BrowserContext;
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
    ExtensionRegistry* registry) {
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

  const Extension* extension =
      registry->enabled_extensions().GetByID(url.host());
  if (extension && AppIsolationInfo::HasIsolatedStorage(extension))
    return PRIV_ISOLATED;
  if (extension && extension->is_hosted_app())
    return PRIV_HOSTED;
  return PRIV_EXTENSION;
}

RenderProcessHostPrivilege GetProcessPrivilege(
    content::RenderProcessHost* process_host,
    ProcessMap* process_map,
    ExtensionRegistry* registry) {
  std::set<std::string> extension_ids =
      process_map->GetExtensionsInProcess(process_host->GetID());
  if (extension_ids.empty())
    return PRIV_NORMAL;

  for (const std::string& extension_id : extension_ids) {
    const Extension* extension =
        registry->enabled_extensions().GetByID(extension_id);
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
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (!registry)
    return url;

  const Extension* extension =
      registry->enabled_extensions().GetHostedAppByURL(url);
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

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (!registry)
    return false;

  const Extension* extension =
      registry->enabled_extensions().GetByID(effective_url.host());
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
bool ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (effective_site_url.SchemeIs(extensions::kExtensionScheme)) {
    // --isolate-extensions should isolate extensions, except for hosted apps.
    // Isolating hosted apps is a good idea, but ought to be a separate knob.
    if (IsIsolateExtensionsEnabled()) {
      const Extension* extension =
          ExtensionRegistry::Get(browser_context)
              ->enabled_extensions()
              .GetExtensionOrAppByURL(effective_site_url);
      if (extension && !extension->is_hosted_app())
        return true;
    }
  }
  return false;
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldLockToOrigin(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  // https://crbug.com/160576 workaround: Origin lock to the chrome-extension://
  // scheme for a hosted app would kill processes on legitimate requests for the
  // app's cookies.
  if (effective_site_url.SchemeIs(extensions::kExtensionScheme)) {
    const Extension* extension =
        ExtensionRegistry::Get(browser_context)
            ->enabled_extensions()
            .GetExtensionOrAppByURL(effective_site_url);
    if (extension && extension->is_hosted_app())
      return false;
  }
  return true;
}

// static
bool ChromeContentBrowserClientExtensionsPart::CanCommitURL(
    content::RenderProcessHost* process_host, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to let most extension URLs commit in any process, since this can
  // be allowed due to web_accessible_resources.  Most hosted app URLs may also
  // load in any process (e.g., in an iframe).  However, the Chrome Web Store
  // cannot be loaded in iframes and should never be requested outside its
  // process.
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(process_host->GetBrowserContext());
  if (!registry)
    return true;

  const Extension* new_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(url);
  if (new_extension && new_extension->is_hosted_app() &&
      new_extension->id() == kWebStoreAppId &&
      !ProcessMap::Get(process_host->GetBrowserContext())
           ->Contains(new_extension->id(), process_host->GetID())) {
    return false;
  }
  return true;
}

bool ChromeContentBrowserClientExtensionsPart::IsIllegalOrigin(
    content::ResourceContext* resource_context,
    int child_process_id,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Consider non-extension URLs safe; they will be checked elsewhere.
  if (!origin.SchemeIs(kExtensionScheme))
    return false;

  // If there is no extension installed for the URL, it couldn't have committed.
  // (If the extension was recently uninstalled, the tab would have closed.)
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
  const Extension* extension =
      extension_info_map->extensions().GetExtensionOrAppByURL(origin);
  if (!extension)
    return true;

  // Check for platform app origins.  These can only be committed by the app
  // itself, or by one if its guests if there are accessible_resources.
  const ProcessMap& process_map = extension_info_map->process_map();
  if (extension->is_platform_app() &&
      !process_map.Contains(extension->id(), child_process_id)) {
    // This is a platform app origin not in the app's own process.  If there are
    // no accessible resources, this is illegal.
    if (!extension->GetManifestData(manifest_keys::kWebviewAccessibleResources))
      return true;

    // If there are accessible resources, the origin is only legal if the given
    // process is a guest of the app.
    std::string owner_extension_id;
    int owner_process_id;
    WebViewRendererState::GetInstance()->GetOwnerInfo(
        child_process_id, &owner_process_id, &owner_extension_id);
    const Extension* owner_extension =
        extension_info_map->extensions().GetByID(owner_extension_id);
    return !owner_extension || owner_extension != extension;
  }

  // With only the origin and not the full URL, we don't have enough information
  // to validate hosted apps or web_accessible_resources in normal extensions.
  // Assume they're legal.
  return false;
}

// static
bool ChromeContentBrowserClientExtensionsPart::IsSuitableHost(
    Profile* profile,
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  DCHECK(profile);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  ProcessMap* process_map = ProcessMap::Get(profile);

  // These may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!registry || !process_map)
    return true;

  // Otherwise, just make sure the process privilege matches the privilege
  // required by the site.
  RenderProcessHostPrivilege privilege_required =
      GetPrivilegeRequiredByUrl(site_url, registry);
  return GetProcessPrivilege(process_host, process_map, registry) ==
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
  ExtensionRegistry* registry =
      profile ? ExtensionRegistry::Get(profile) : NULL;
  if (!registry)
    return false;

  // We have to have a valid extension with background page to proceed.
  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(url);
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
    ProcessManager* epm = ProcessManager::Get(profiles[i]);
    for (ExtensionHost* host : epm->background_hosts())
      process_ids.insert(host->render_process_host()->GetID());
  }

  return (process_ids.size() >
          (max_process_count * chrome::kMaxShareOfExtensionProcesses));
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldSwapBrowsingInstancesForNavigation(SiteInstance* site_instance,
                                             const GURL& current_url,
                                             const GURL& new_url) {
  // If we don't have an ExtensionRegistry, then rely on the SiteInstance logic
  // in RenderFrameHostManager to decide when to swap.
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!registry)
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
      registry->enabled_extensions().GetExtensionOrAppByURL(current_url);
  if (current_extension && current_extension->is_hosted_app() &&
      current_extension->id() != kWebStoreAppId)
    current_extension = NULL;

  const Extension* new_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(new_url);
  if (new_extension && new_extension->is_hosted_app() &&
      new_extension->id() != kWebStoreAppId)
    new_extension = NULL;

  // First do a process check.  We should force a BrowsingInstance swap if the
  // current process doesn't know about new_extension, even if current_extension
  // is somehow the same as new_extension.
  ProcessMap* process_map = ProcessMap::Get(site_instance->GetBrowserContext());
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
bool ChromeContentBrowserClientExtensionsPart::AllowServiceWorker(
    const GURL& scope,
    const GURL& first_party_url,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id) {
  // We only care about extension urls.
  // TODO(devlin): Also address chrome-extension-resource.
  if (!first_party_url.SchemeIs(kExtensionScheme))
    return true;

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
  const Extension* extension =
      extension_info_map->extensions().GetExtensionOrAppByURL(first_party_url);
  // Don't allow a service worker for an extension url with no extension (this
  // could happen in the case of, e.g., an unloaded extension).
  return extension != nullptr;
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
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
    if (!registry) {
      *result = true;
      return true;
    }
    const Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(to_url);
    if (!extension) {
      *result = true;
      return true;
    }
    const Extension* from_extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(
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

void ChromeContentBrowserClientExtensionsPart::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->AddFilter(new ChromeExtensionMessageFilter(id, profile));
  host->AddFilter(new ExtensionMessageFilter(id, profile));
  host->AddFilter(new IOThreadExtensionMessageFilter(id, profile));
  host->AddFilter(new ExtensionsGuestViewMessageFilter(id, profile));
  extension_web_request_api_helpers::SendExtensionWebRequestStatusToHost(host);
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  BrowserContext* context = site_instance->GetProcess()->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry)
    return;

  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          site_instance->GetSiteURL());
  if (!extension)
    return;

  ProcessMap::Get(context)->Insert(extension->id(),
                                   site_instance->GetProcess()->GetID(),
                                   site_instance->GetId());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::RegisterExtensionProcess,
                 ExtensionSystem::Get(context)->info_map(), extension->id(),
                 site_instance->GetProcess()->GetID(), site_instance->GetId()));
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceDeleting(
    SiteInstance* site_instance) {
  BrowserContext* context = site_instance->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry)
    return;

  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          site_instance->GetSiteURL());
  if (!extension)
    return;

  ProcessMap::Get(context)->Remove(extension->id(),
                                   site_instance->GetProcess()->GetID(),
                                   site_instance->GetId());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::UnregisterExtensionProcess,
                 ExtensionSystem::Get(context)->info_map(), extension->id(),
                 site_instance->GetProcess()->GetID(), site_instance->GetId()));
}

void ChromeContentBrowserClientExtensionsPart::OverrideWebkitPrefs(
    RenderViewHost* rvh,
    WebPreferences* web_prefs) {
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(rvh->GetProcess()->GetBrowserContext());
  if (!registry)
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
  const Extension* extension =
      registry->enabled_extensions().GetByID(site_url.host());
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
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableMojoSerialService)) {
      command_line->AppendSwitch(switches::kEnableMojoSerialService);
    }
  }
}

}  // namespace extensions
