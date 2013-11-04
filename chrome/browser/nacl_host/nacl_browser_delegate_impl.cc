// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"

#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/url_pattern.h"
#include "ppapi/c/private/ppb_nacl_private.h"

using extensions::SharedModuleInfo;

NaClBrowserDelegateImpl::NaClBrowserDelegateImpl(
    ExtensionInfoMap* extension_info_map)
    : extension_info_map_(extension_info_map),
      inverse_debug_patterns_(false) {
}

NaClBrowserDelegateImpl::~NaClBrowserDelegateImpl() {
}

void NaClBrowserDelegateImpl::ShowNaClInfobar(int render_process_id,
                                              int render_view_id,
                                              int error_id) {
  DCHECK_EQ(PP_NACL_MANIFEST_MISSING_ARCH, error_id);
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
    URLPattern pattern;
    if (pattern.Parse(*iter) == URLPattern::PARSE_SUCCESS) {
      // If URL pattern has scheme equal to *, Parse method resets valid
      // schemes mask to http and https only, so we need to reset it after
      // Parse to include chrome-extension scheme that can be used by NaCl
      // manifest files.
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
    const GURL& file_url, bool use_blocking_api, base::FilePath* file_path) {
  DCHECK(extension_info_map_);
  // Check that the URL is recognized by the extension system.
  const extensions::Extension* extension =
      extension_info_map_->extensions().GetExtensionOrAppByURL(file_url);
  if (!extension)
    return false;

  // This is a short-cut which avoids calling a blocking file operation
  // (GetFilePath()), so that this can be called on the IO thread. It only
  // handles a subset of the urls.
  if (!use_blocking_api) {
    if (file_url.SchemeIs(extensions::kExtensionScheme)) {
      std::string path = file_url.path();
      TrimString(path, "/", &path);  // Remove first slash
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
        extension_info_map_->extensions().GetByID(new_extension_id);
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
