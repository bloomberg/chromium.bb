// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "components/nacl/common/nacl_browser_delegate.h"

class NaClBrowserDelegateImpl : public NaClBrowserDelegate {
 public:
  explicit NaClBrowserDelegateImpl(ExtensionInfoMap* extension_info_map);
  virtual ~NaClBrowserDelegateImpl();

  virtual void ShowNaClInfobar(int render_process_id, int render_view_id,
                               int error_id) OVERRIDE;
  virtual bool DialogsAreSuppressed() OVERRIDE;
  virtual bool GetCacheDirectory(base::FilePath* cache_dir) OVERRIDE;
  virtual bool GetPluginDirectory(base::FilePath* plugin_dir) OVERRIDE;
  virtual bool GetPnaclDirectory(base::FilePath* pnacl_dir) OVERRIDE;
  virtual bool GetUserDirectory(base::FilePath* user_dir) OVERRIDE;
  virtual std::string GetVersionString() const OVERRIDE;
  virtual ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) OVERRIDE;
  virtual void TryInstallPnacl(
      const base::Callback<void(bool)>& installed) OVERRIDE;
  virtual bool MapUrlToLocalFilePath(const GURL& url,
                                     bool is_blocking,
                                     base::FilePath* file_path) OVERRIDE;
  virtual void SetDebugPatterns(std::string debug_patterns) OVERRIDE;
  virtual bool URLMatchesDebugPatterns(const GURL& manifest_url) OVERRIDE;

 private:
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  std::vector<URLPattern> debug_patterns_;
  bool inverse_debug_patterns_;
  DISALLOW_COPY_AND_ASSIGN(NaClBrowserDelegateImpl);
};


#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_
