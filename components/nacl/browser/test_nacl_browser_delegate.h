// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_TEST_NACL_BROWSER_DELEGATE_H_
#define COMPONENTS_NACL_BROWSER_TEST_NACL_BROWSER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/nacl/browser/nacl_browser_delegate.h"

// This is a base test implementation of NaClBrowserDelegate which
// does nothing. Individual tests can override the methods further.
// To use the test delegate:
//
//   NaClBrowser::SetDelegate(new RefinedTestNaClBrowserDelegate);
//
// and
//
//   NaClBrowser::SetDelegate(NULL);   // frees the test delegate.
class TestNaClBrowserDelegate : public NaClBrowserDelegate {
 public:
  TestNaClBrowserDelegate();
  virtual ~TestNaClBrowserDelegate();
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
                                     bool use_blocking_api,
                                     const base::FilePath& profile_directory,
                                     base::FilePath* file_path) OVERRIDE;
  virtual void SetDebugPatterns(std::string debug_patterns) OVERRIDE;
  virtual bool URLMatchesDebugPatterns(const GURL& manifest_url) OVERRIDE;
  virtual content::BrowserPpapiHost::OnKeepaliveCallback
      GetOnKeepaliveCallback() OVERRIDE;
  virtual bool IsNonSfiModeAllowed(const base::FilePath& profile_directory,
                                   const GURL& manifest_url) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNaClBrowserDelegate);
};

#endif  // COMPONENTS_NACL_BROWSER_TEST_NACL_BROWSER_DELEGATE_H_
