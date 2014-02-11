// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/view_type.h"

#if defined(OS_MACOSX)
#include "chrome/browser/extensions/extension_view_host_mac.h"
#endif

namespace extensions {

namespace {

// Creates a new ExtensionHost with its associated view, grouping it in the
// appropriate SiteInstance (and therefore process) based on the URL and
// profile.
ExtensionViewHost* CreateViewHostForExtension(const Extension* extension,
                                              const GURL& url,
                                              Profile* profile,
                                              Browser* browser,
                                              ViewType view_type) {
  DCHECK(profile);
  // A NULL browser may only be given for dialogs.
  DCHECK(browser || view_type == VIEW_TYPE_EXTENSION_DIALOG);
  ProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  content::SiteInstance* site_instance = pm->GetSiteInstanceForURL(url);
  ExtensionViewHost* host =
#if defined(OS_MACOSX)
      new ExtensionViewHostMac(extension, site_instance, url, view_type);
#else
      new ExtensionViewHost(extension, site_instance, url, view_type);
#endif
  host->CreateView(browser);
  return host;
}

// Creates a view host for an extension in an incognito window. Returns NULL
// if the extension is not allowed to run in incognito.
ExtensionViewHost* CreateViewHostForIncognito(const Extension* extension,
                                              const GURL& url,
                                              Profile* profile,
                                              Browser* browser,
                                              ViewType view_type) {
  DCHECK(extension);
  DCHECK(profile->IsOffTheRecord());

  if (!IncognitoInfo::IsSplitMode(extension)) {
    // If it's not split-mode the host is associated with the original profile.
    Profile* original_profile = profile->GetOriginalProfile();
    return CreateViewHostForExtension(
        extension, url, original_profile, browser, view_type);
  }

  // Create the host if the extension can run in incognito.
  if (util::IsIncognitoEnabled(extension->id(), profile)) {
    return CreateViewHostForExtension(
        extension, url, profile, browser, view_type);
  }
  NOTREACHED() <<
      "We shouldn't be trying to create an incognito extension view unless "
      "it has been enabled for incognito.";
  return NULL;
}

// Returns the extension associated with |url| in |profile|. Returns NULL if
// the extension does not exist.
const Extension* GetExtensionForUrl(Profile* profile, const GURL& url) {
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return NULL;
  std::string extension_id = url.host();
  if (url.SchemeIs(content::kChromeUIScheme) &&
      url.host() == chrome::kChromeUIExtensionInfoHost)
    extension_id = url.path().substr(1);
  return service->extensions()->GetByID(extension_id);
}

// Creates and initializes an ExtensionViewHost for the extension with |url|.
ExtensionViewHost* CreateViewHost(const GURL& url,
                                  Profile* profile,
                                  Browser* browser,
                                  extensions::ViewType view_type) {
  DCHECK(profile);
  // A NULL browser may only be given for dialogs.
  DCHECK(browser || view_type == VIEW_TYPE_EXTENSION_DIALOG);

  const Extension* extension = GetExtensionForUrl(profile, url);
  if (!extension)
    return NULL;
  if (profile->IsOffTheRecord()) {
    return CreateViewHostForIncognito(
        extension, url, profile, browser, view_type);
  }
  return CreateViewHostForExtension(
      extension, url, profile, browser, view_type);
}

}  // namespace

// static
ExtensionViewHost* ExtensionViewHostFactory::CreatePopupHost(const GURL& url,
                                                             Browser* browser) {
  DCHECK(browser);
  return CreateViewHost(
      url, browser->profile(), browser, VIEW_TYPE_EXTENSION_POPUP);
}

// static
ExtensionViewHost* ExtensionViewHostFactory::CreateInfobarHost(
    const GURL& url,
    Browser* browser) {
  DCHECK(browser);
  return CreateViewHost(
      url, browser->profile(), browser, VIEW_TYPE_EXTENSION_INFOBAR);
}

// static
ExtensionViewHost* ExtensionViewHostFactory::CreateDialogHost(
    const GURL& url,
    Profile* profile) {
  DCHECK(profile);
  return CreateViewHost(url, profile, NULL, VIEW_TYPE_EXTENSION_DIALOG);
}

}  // namespace extensions
