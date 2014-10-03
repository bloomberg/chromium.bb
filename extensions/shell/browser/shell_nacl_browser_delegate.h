// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NACL_BROWSER_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NACL_BROWSER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/nacl/browser/nacl_browser_delegate.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class InfoMap;

// A lightweight NaClBrowserDelegate for app_shell. Only supports a single
// BrowserContext.
class ShellNaClBrowserDelegate : public NaClBrowserDelegate {
 public:
  // Uses |context| to look up extensions via InfoMap on the IO thread.
  explicit ShellNaClBrowserDelegate(content::BrowserContext* context);
  virtual ~ShellNaClBrowserDelegate();

  // NaClBrowserDelegate overrides:
  virtual void ShowMissingArchInfobar(int render_process_id,
                                      int render_view_id) override;
  virtual bool DialogsAreSuppressed() override;
  virtual bool GetCacheDirectory(base::FilePath* cache_dir) override;
  virtual bool GetPluginDirectory(base::FilePath* plugin_dir) override;
  virtual bool GetPnaclDirectory(base::FilePath* pnacl_dir) override;
  virtual bool GetUserDirectory(base::FilePath* user_dir) override;
  virtual std::string GetVersionString() const override;
  virtual ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) override;
  virtual bool MapUrlToLocalFilePath(const GURL& url,
                                     bool is_blocking,
                                     const base::FilePath& profile_directory,
                                     base::FilePath* file_path) override;
  virtual void SetDebugPatterns(std::string debug_patterns) override;
  virtual bool URLMatchesDebugPatterns(const GURL& manifest_url) override;
  virtual content::BrowserPpapiHost::OnKeepaliveCallback
      GetOnKeepaliveCallback() override;
  virtual bool IsNonSfiModeAllowed(const base::FilePath& profile_directory,
                                   const GURL& manifest_url) override;

 private:
  content::BrowserContext* browser_context_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(ShellNaClBrowserDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NACL_BROWSER_DELEGATE_H_
