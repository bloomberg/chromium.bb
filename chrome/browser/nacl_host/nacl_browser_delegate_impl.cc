// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"

#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pepper_permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/url_pattern.h"
#include "ppapi/c/private/ppb_nacl_private.h"

using extensions::SharedModuleInfo;

namespace {

// These are temporarily needed for testing non-sfi mode on ChromeOS without
// passing command-line arguments to Chrome.
const char* const kAllowedNonSfiOrigins[] = {
    "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see http://crbug.com/355141
    "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see http://crbug.com/355141
};

// Handles an extension's NaCl process transitioning in or out of idle state by
// relaying the state to the extension's process manager.
//
// A NaCl instance, when active (making PPAPI calls or receiving callbacks),
// sends keepalive IPCs to the browser process BrowserPpapiHost at a throttled
// rate. The content::BrowserPpapiHost passes context information up to the
// chrome level NaClProcessHost where we use the instance's context to find the
// associated extension process manager.
//
// There is a 1:many relationship for extension:nacl-embeds, but only a
// 1:1 relationship for NaClProcessHost:PP_Instance. The content layer doesn't
// rely on this knowledge because it routes messages for ppapi non-nacl
// instances as well, though they won't have callbacks set. Here the 1:1
// assumption is made and DCHECKed.
void OnKeepaliveOnUIThread(
    const content::BrowserPpapiHost::OnKeepaliveInstanceData& instance_data,
    const base::FilePath& profile_data_directory) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Only one instance will exist for NaCl embeds, even when more than one
  // embed of the same plugin exists on the same page.
  DCHECK(instance_data.size() == 1);
  if (instance_data.size() < 1)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(
          instance_data[0].render_process_id, instance_data[0].render_frame_id);
  if (!render_frame_host)
    return;

  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance)
    return;

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(site_instance->GetBrowserContext());
  if (!extension_system)
    return;

  const ExtensionService* extension_service =
      extension_system->extension_service();
  if (!extension_service)
    return;

  const extensions::Extension* extension = extension_service->GetExtensionById(
      instance_data[0].document_url.host(), false);
  if (!extension)
    return;

  extensions::ProcessManager* pm = extension_system->process_manager();
  if (!pm)
    return;

  pm->KeepaliveImpulse(extension);
}

// Calls OnKeepaliveOnUIThread on UI thread.
void OnKeepalive(
    const content::BrowserPpapiHost::OnKeepaliveInstanceData& instance_data,
    const base::FilePath& profile_data_directory) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(&OnKeepaliveOnUIThread,
                                              instance_data,
                                              profile_data_directory));
}

}  // namespace

NaClBrowserDelegateImpl::NaClBrowserDelegateImpl(
    ProfileManager* profile_manager)
    : profile_manager_(profile_manager), inverse_debug_patterns_(false) {
  DCHECK(profile_manager_);
  for (size_t i = 0; i < arraysize(kAllowedNonSfiOrigins); ++i) {
    allowed_nonsfi_origins_.insert(kAllowedNonSfiOrigins[i]);
  }
}

NaClBrowserDelegateImpl::~NaClBrowserDelegateImpl() {
}

void NaClBrowserDelegateImpl::ShowMissingArchInfobar(int render_process_id,
                                                     int render_view_id) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&NaClInfoBarDelegate::Create, render_process_id,
                 render_view_id));
}

bool NaClBrowserDelegateImpl::DialogsAreSuppressed() {
  return logging::DialogsAreSuppressed();
}

bool NaClBrowserDelegateImpl::GetCacheDirectory(base::FilePath* cache_dir) {
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  chrome::GetUserCacheDirectory(user_data_dir, cache_dir);
  return true;
}

bool NaClBrowserDelegateImpl::GetPluginDirectory(base::FilePath* plugin_dir) {
  return PathService::Get(chrome::DIR_INTERNAL_PLUGINS, plugin_dir);
}

bool NaClBrowserDelegateImpl::GetPnaclDirectory(base::FilePath* pnacl_dir) {
  return PathService::Get(chrome::DIR_PNACL_COMPONENT, pnacl_dir);
}

bool NaClBrowserDelegateImpl::GetUserDirectory(base::FilePath* user_dir) {
  return PathService::Get(chrome::DIR_USER_DATA, user_dir);
}

std::string NaClBrowserDelegateImpl::GetVersionString() const {
  return chrome::VersionInfo().CreateVersionString();
}

ppapi::host::HostFactory* NaClBrowserDelegateImpl::CreatePpapiHostFactory(
    content::BrowserPpapiHost* ppapi_host) {
  return new chrome::ChromeBrowserPepperHostFactory(ppapi_host);
}

void NaClBrowserDelegateImpl::SetDebugPatterns(std::string debug_patterns) {
  if (!debug_patterns.empty() && debug_patterns[0] == '!') {
    inverse_debug_patterns_ = true;
    debug_patterns.erase(0, 1);
  }
  if (debug_patterns.empty()) {
    return;
  }
  std::vector<std::string> patterns;
  base::SplitString(debug_patterns, ',', &patterns);
  for (std::vector<std::string>::iterator iter = patterns.begin();
       iter != patterns.end(); ++iter) {
    // Allow chrome:// schema, which is used to filter out the internal
    // PNaCl translator. Also allow chrome-extension:// schema (which
    // can have NaCl modules). The default is to disallow these schema
    // since they can be dangerous in the context of chrome extension
    // permissions, but they are okay here, for NaCl GDB avoidance.
    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(*iter) == URLPattern::PARSE_SUCCESS) {
      // If URL pattern has scheme equal to *, Parse method resets valid
      // schemes mask to http and https only, so we need to reset it after
      // Parse to re-include chrome-extension and chrome schema.
      pattern.SetValidSchemes(URLPattern::SCHEME_ALL);
      debug_patterns_.push_back(pattern);
    }
  }
}

bool NaClBrowserDelegateImpl::URLMatchesDebugPatterns(
    const GURL& manifest_url) {
  // Empty patterns are forbidden so we ignore them.
  if (debug_patterns_.empty()) {
    return true;
  }
  bool matches = false;
  for (std::vector<URLPattern>::iterator iter = debug_patterns_.begin();
       iter != debug_patterns_.end(); ++iter) {
    if (iter->MatchesURL(manifest_url)) {
      matches = true;
      break;
    }
  }
  if (inverse_debug_patterns_) {
    return !matches;
  } else {
    return matches;
  }
}

// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
bool NaClBrowserDelegateImpl::MapUrlToLocalFilePath(
    const GURL& file_url,
    bool use_blocking_api,
    const base::FilePath& profile_directory,
    base::FilePath* file_path) {
  scoped_refptr<extensions::InfoMap> extension_info_map =
      GetExtensionInfoMap(profile_directory);
  // Check that the URL is recognized by the extension system.
  const extensions::Extension* extension =
      extension_info_map->extensions().GetExtensionOrAppByURL(file_url);
  if (!extension)
    return false;

  // This is a short-cut which avoids calling a blocking file operation
  // (GetFilePath()), so that this can be called on the IO thread. It only
  // handles a subset of the urls.
  if (!use_blocking_api) {
    if (file_url.SchemeIs(extensions::kExtensionScheme)) {
      std::string path = file_url.path();
      base::TrimString(path, "/", &path);  // Remove first slash
      *file_path = extension->path().AppendASCII(path);
      return true;
    }
    return false;
  }

  std::string path = file_url.path();
  extensions::ExtensionResource resource;

  if (SharedModuleInfo::IsImportedPath(path)) {
    // Check if this is a valid path that is imported for this extension.
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const extensions::Extension* new_extension =
        extension_info_map->extensions().GetByID(new_extension_id);
    if (!new_extension)
      return false;

    if (!SharedModuleInfo::ImportsExtensionById(extension, new_extension_id) ||
        !SharedModuleInfo::IsExportAllowed(new_extension, new_relative_path)) {
      return false;
    }

    resource = new_extension->GetResource(new_relative_path);
  } else {
    // Check that the URL references a resource in the extension.
    resource = extension->GetResource(path);
  }

  if (resource.empty())
    return false;

  // GetFilePath is a blocking function call.
  const base::FilePath resource_file_path = resource.GetFilePath();
  if (resource_file_path.empty())
    return false;

  *file_path = resource_file_path;
  return true;
}

content::BrowserPpapiHost::OnKeepaliveCallback
NaClBrowserDelegateImpl::GetOnKeepaliveCallback() {
  return base::Bind(&OnKeepalive);
}

bool NaClBrowserDelegateImpl::IsNonSfiModeAllowed(
    const base::FilePath& profile_directory,
    const GURL& manifest_url) {
  const extensions::ExtensionSet* extension_set =
      &GetExtensionInfoMap(profile_directory)->extensions();
  return chrome::IsExtensionOrSharedModuleWhitelisted(
      manifest_url, extension_set, allowed_nonsfi_origins_);
}

scoped_refptr<extensions::InfoMap> NaClBrowserDelegateImpl::GetExtensionInfoMap(
    const base::FilePath& profile_directory) {
  // Get the profile associated with the renderer.
  Profile* profile = profile_manager_->GetProfileByPath(profile_directory);
  DCHECK(profile);
  scoped_refptr<extensions::InfoMap> extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
  DCHECK(extension_info_map);
  return extension_info_map;
}
