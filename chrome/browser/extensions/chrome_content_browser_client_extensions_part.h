// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"

namespace content {
class ResourceContext;
}

namespace extensions {

class BrowserPermissionsPolicyDelegate;

// Implements the extensions portion of ChromeContentBrowserClient.
class ChromeContentBrowserClientExtensionsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientExtensionsPart();
  virtual ~ChromeContentBrowserClientExtensionsPart();

  // Corresponds to the ChromeContentBrowserClient function of the same name.
  static GURL GetEffectiveURL(Profile* profile, const GURL& url);
  static bool ShouldUseProcessPerSite(Profile* profile,
                                      const GURL& effective_url);
  static bool CanCommitURL(content::RenderProcessHost* process_host,
                           const GURL& url);
  static bool IsSuitableHost(Profile* profile,
                             content::RenderProcessHost* process_host,
                             const GURL& site_url);
  static bool ShouldTryToUseExistingProcessHost(Profile* profile,
                                                const GURL& url);
  static bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url);
  static bool ShouldSwapProcessesForRedirect(
      content::ResourceContext* resource_context,
      const GURL& current_url,
      const GURL& new_url);

  // Similiar to ChromeContentBrowserClient::ShouldAllowOpenURL(), but the
  // return value indicates whether to use |result| or not.
  static bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                                 const GURL& from_url,
                                 const GURL& to_url,
                                 bool* result);

  // Helper function to call InfoMap::SetSigninProcess().
  static void SetSigninProcess(content::SiteInstance* site_instance);

 private:
  // ChromeContentBrowserClientParts:
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) OVERRIDE;
  virtual void SiteInstanceGotProcess(
      content::SiteInstance* site_instance) OVERRIDE;
  virtual void SiteInstanceDeleting(
      content::SiteInstance* site_instance) OVERRIDE;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   content::WebPreferences* web_prefs) OVERRIDE;
  virtual void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) OVERRIDE;
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_allowed_schemes) OVERRIDE;
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) OVERRIDE;
  virtual void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      ScopedVector<storage::FileSystemBackend>* additional_backends) OVERRIDE;
  virtual void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost* process,
      Profile* profile) OVERRIDE;

  scoped_ptr<BrowserPermissionsPolicyDelegate> permissions_policy_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientExtensionsPart);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_

