// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "extensions/common/url_pattern.h"

namespace extensions {
class InfoMap;
}

class ProfileManager;

class NaClBrowserDelegateImpl : public NaClBrowserDelegate {
 public:
  explicit NaClBrowserDelegateImpl(ProfileManager* profile_manager);
  virtual ~NaClBrowserDelegateImpl();

  virtual void ShowMissingArchInfobar(int render_process_id,
                                      int render_view_id) OVERRIDE;
  virtual bool DialogsAreSuppressed() OVERRIDE;
  virtual bool GetCacheDirectory(base::FilePath* cache_dir) OVERRIDE;
  virtual bool GetPluginDirectory(base::FilePath* plugin_dir) OVERRIDE;
  virtual bool GetPnaclDirectory(base::FilePath* pnacl_dir) OVERRIDE;
  virtual bool GetUserDirectory(base::FilePath* user_dir) OVERRIDE;
  virtual std::string GetVersionString() const OVERRIDE;
  virtual ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) OVERRIDE;
  virtual bool MapUrlToLocalFilePath(const GURL& url,
                                     bool is_blocking,
                                     const base::FilePath& profile_directory,
                                     base::FilePath* file_path) OVERRIDE;
  virtual void SetDebugPatterns(std::string debug_patterns) OVERRIDE;
  virtual bool URLMatchesDebugPatterns(const GURL& manifest_url) OVERRIDE;
  virtual content::BrowserPpapiHost::OnKeepaliveCallback
      GetOnKeepaliveCallback() OVERRIDE;
  virtual bool IsNonSfiModeAllowed(const base::FilePath& profile_directory,
                                   const GURL& manifest_url) OVERRIDE;

 private:
  scoped_refptr<extensions::InfoMap> GetExtensionInfoMap(
      const base::FilePath& profile_directory);
  ProfileManager* profile_manager_;
  std::vector<URLPattern> debug_patterns_;
  bool inverse_debug_patterns_;
  std::set<std::string> allowed_nonsfi_origins_;
  DISALLOW_COPY_AND_ASSIGN(NaClBrowserDelegateImpl);
};


#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_
