// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer.h"

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace {

const char kInvalidIdError[] = "Invalid id";
const char kNoBrowserError[] = "No browser found";

const char kInlineInstallSource[] = "inline";
const char kDefaultInstallSource[] = "";

GURL GetWebstoreInstallUrl(
    const std::string& extension_id, const std::string& install_source) {
  std::vector<std::string> params;
  params.push_back("id=" + extension_id);
  if (!install_source.empty()) {
    params.push_back("installsource=" + install_source);
  }
  params.push_back("lang=" + g_browser_process->GetApplicationLocale());
  params.push_back("uc");
  std::string url_string = extension_urls::GetWebstoreUpdateUrl(true).spec();

  GURL url(url_string + "?response=redirect&x=" +
      net::EscapeQueryParamValue(JoinString(params, '&'), true));
  DCHECK(url.is_valid());

  return url;
}

}  // namespace


WebstoreInstaller::WebstoreInstaller(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::Source<CrxInstaller>(NULL));
}

WebstoreInstaller::~WebstoreInstaller() {}

struct WebstoreInstaller::PendingInstall {
  PendingInstall() : delegate(NULL) {}
  PendingInstall(const std::string& id, const GURL& url, Delegate* delegate)
      : id(id), download_url(url), delegate(delegate) {}
  ~PendingInstall() {}

  // The id of the extension.
  std::string id;

  // The gallery download URL for the extension.
  GURL download_url;

  // The delegate for this install.
  Delegate* delegate;
};

void WebstoreInstaller::InstallExtension(
    const std::string& id, Delegate* delegate, int flags) {
  if (!Extension::IdIsValid(id)) {
    ReportFailure(id, kInvalidIdError);
    return;
  }

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser) {
    ReportFailure(id, kNoBrowserError);
    return;
  }

  GURL install_url = GetWebstoreInstallUrl(id, flags & FLAG_INLINE_INSTALL ?
      kInlineInstallSource : kDefaultInstallSource);
  PendingInstall pending_install = CreatePendingInstall(
      id, install_url, delegate);

  // The download url for the given |id| is now contained in
  // |pending_install.download_url|. We navigate the current tab to this url
  // which will result in a download starting. Once completed it will go through
  // the normal extension install flow.
  NavigationController& controller =
      browser->tabstrip_model()->GetActiveTabContents()->controller();

  // TODO(mihaip, jstritar): For inline installs, we pretend like the referrer
  // is the gallery, even though this could be an inline install, in order to
  // pass the checks in ExtensionService::IsDownloadFromGallery. We should
  // instead pass the real referrer, track if this is an inline install in the
  // whitelist entry and look that up when checking that this is a valid
  // download.
  GURL referrer = controller.GetActiveEntry()->url();
  if (flags & FLAG_INLINE_INSTALL)
    referrer = GURL(extension_urls::GetWebstoreItemDetailURLPrefix() + id);

  controller.LoadURL(install_url,
                     referrer,
                     content::PAGE_TRANSITION_LINK,
                     std::string());
}

void WebstoreInstaller::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      CHECK(profile_->IsSameProfile(content::Source<Profile>(source).ptr()));
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      ReportSuccess(extension->id());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      CrxInstaller* crx_installer = content::Source<CrxInstaller>(source).ptr();
      CHECK(crx_installer);
      if (!profile_->IsSameProfile(crx_installer->profile()))
        return;

      std::string id = GetPendingInstallId(
          crx_installer->original_download_url());
      const std::string* error =
          content::Details<const std::string>(details).ptr();
      if (!id.empty())
        ReportFailure(id, *error);
      break;
    }

    default:
      NOTREACHED();
  }
}

bool WebstoreInstaller::ClearPendingInstall(
    const std::string& id, PendingInstall* install) {
  for (size_t i = 0; i < pending_installs_.size(); ++i) {
    if (pending_installs_[i].id == id) {
      *install = pending_installs_[i];
      pending_installs_.erase(pending_installs_.begin() + i);
      return true;
    }
  }
  return false;
}

const WebstoreInstaller::PendingInstall&
    WebstoreInstaller::CreatePendingInstall(
        const std::string& id, GURL install_url, Delegate* delegate) {
  PendingInstall pending_install(id, install_url, delegate);
  pending_installs_.push_back(pending_install);

  return *(pending_installs_.end() - 1);
}


std::string WebstoreInstaller::GetPendingInstallId(const GURL& url) {
  if (url.is_empty())
    return std::string();

  for (size_t i = 0; i < pending_installs_.size(); ++i) {
    if (pending_installs_[i].download_url == url)
      return pending_installs_[i].id;
  }

  return std::string();
}

void WebstoreInstaller::ReportFailure(
    const std::string& id, const std::string& error) {
  PendingInstall install;
  if (!ClearPendingInstall(id, &install))
    return;

  if (install.delegate)
    install.delegate->OnExtensionInstallFailure(id, error);
}

void WebstoreInstaller::ReportSuccess(const std::string& id) {
  PendingInstall install;
  if (!ClearPendingInstall(id, &install))
    return;

  if (install.delegate)
    install.delegate->OnExtensionInstallSuccess(id);
}
