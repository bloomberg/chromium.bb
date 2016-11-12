// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/pnacl_component_installer.h"
#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pepper_permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/features/features.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/url_pattern.h"
#endif

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only one instance will exist for NaCl embeds, even when more than one
  // embed of the same plugin exists on the same page.
  DCHECK_EQ(1U, instance_data.size());
  if (instance_data.size() < 1)
    return;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ProcessManager::OnKeepaliveFromPlugin(
      instance_data[0].render_process_id,
      instance_data[0].render_frame_id,
      instance_data[0].document_url.host());
#endif
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
  return chrome::GetVersionString();
}

ppapi::host::HostFactory* NaClBrowserDelegateImpl::CreatePpapiHostFactory(
    content::BrowserPpapiHost* ppapi_host) {
  return new ChromeBrowserPepperHostFactory(ppapi_host);
}

void NaClBrowserDelegateImpl::SetDebugPatterns(
    const std::string& debug_patterns) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (debug_patterns.empty()) {
    return;
  }
  std::vector<std::string> patterns;
  if (debug_patterns[0] == '!') {
    std::string negated_patterns = debug_patterns;
    inverse_debug_patterns_ = true;
    negated_patterns.erase(0, 1);
    patterns = base::SplitString(
        negated_patterns, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  } else {
    patterns = base::SplitString(
        debug_patterns, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  for (const std::string& pattern_str : patterns) {
    // Allow chrome:// schema, which is used to filter out the internal
    // PNaCl translator. Also allow chrome-extension:// schema (which
    // can have NaCl modules). The default is to disallow these schema
    // since they can be dangerous in the context of chrome extension
    // permissions, but they are okay here, for NaCl GDB avoidance.
    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(pattern_str) == URLPattern::PARSE_SUCCESS) {
      // If URL pattern has scheme equal to *, Parse method resets valid
      // schemes mask to http and https only, so we need to reset it after
      // Parse to re-include chrome-extension and chrome schema.
      pattern.SetValidSchemes(URLPattern::SCHEME_ALL);
      debug_patterns_.push_back(pattern);
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool NaClBrowserDelegateImpl::URLMatchesDebugPatterns(
    const GURL& manifest_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
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
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
bool NaClBrowserDelegateImpl::MapUrlToLocalFilePath(
    const GURL& file_url,
    bool use_blocking_api,
    const base::FilePath& profile_directory,
    base::FilePath* file_path) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::InfoMap> extension_info_map =
      GetExtensionInfoMap(profile_directory);
  return extension_info_map->MapUrlToLocalFilePath(
      file_url, use_blocking_api, file_path);
#else
  return false;
#endif
}

content::BrowserPpapiHost::OnKeepaliveCallback
NaClBrowserDelegateImpl::GetOnKeepaliveCallback() {
  return base::Bind(&OnKeepalive);
}

bool NaClBrowserDelegateImpl::IsNonSfiModeAllowed(
    const base::FilePath& profile_directory,
    const GURL& manifest_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::ExtensionSet* extension_set =
      &GetExtensionInfoMap(profile_directory)->extensions();
  return chrome::IsExtensionOrSharedModuleWhitelisted(
      manifest_url, extension_set, allowed_nonsfi_origins_);
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
scoped_refptr<extensions::InfoMap> NaClBrowserDelegateImpl::GetExtensionInfoMap(
    const base::FilePath& profile_directory) {
  // Get the profile associated with the renderer.
  Profile* profile = profile_manager_->GetProfileByPath(profile_directory);
  DCHECK(profile);
  scoped_refptr<extensions::InfoMap> extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
  DCHECK(extension_info_map.get());
  return extension_info_map;
}
#endif
