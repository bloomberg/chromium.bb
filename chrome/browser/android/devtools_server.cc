// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/devtools_server.h"

#include <cstring>
#include <pwd.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/base/unix_domain_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/static/%s/devtools.html";
const char kSocketName[] = "chrome_devtools_remote";

// Delegate implementation for the devtools http handler on android. A new
// instance of this gets created each time devtools is enabled.
class DevToolsServerDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  DevToolsServerDelegate() {
  }

  virtual std::string GetDiscoveryPageHTML() {
    // TopSites updates itself after a delay. Ask TopSites to update itself
    // when we're about to show the remote debugging landing page.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsServerDelegate::PopulatePageThumbnails));
    return ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_DEVTOOLS_DISCOVERY_PAGE_HTML,
        ui::SCALE_FACTOR_NONE).as_string();
  }

  virtual bool BundlesFrontendResources() {
    return false;
  }

  virtual std::string GetFrontendResourcesBaseURL() {
    return "";
  }

  virtual std::string GetPageThumbnailData(const GURL& url) {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (top_sites) {
      scoped_refptr<base::RefCountedMemory> data;
      if (top_sites->GetPageThumbnail(url, &data))
        return std::string(reinterpret_cast<const char*>(data->front()),
                           data->size());
    }
    return "";
  }

 private:
  static void PopulatePageThumbnails() {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (top_sites)
      top_sites->SyncWithHistory();
  }

  DISALLOW_COPY_AND_ASSIGN(DevToolsServerDelegate);
};

}  // namespace

DevToolsServer::DevToolsServer() : protocol_handler_(NULL) {
}

DevToolsServer::~DevToolsServer() {
  Stop();
}

void DevToolsServer::Start() {
  if (protocol_handler_)
    return;

  chrome::VersionInfo version_info;
  Profile* profile = ProfileManager::GetLastUsedProfile()->GetOriginalProfile();

  protocol_handler_ = content::DevToolsHttpHandler::Start(
      new net::UnixDomainSocketWithAbstractNamespaceFactory(
          kSocketName,
          base::Bind(&content::CanUserConnectToDevTools)),
      StringPrintf(kFrontEndURL, version_info.Version().c_str()),
      profile->GetRequestContext(),
      new DevToolsServerDelegate());
}

void DevToolsServer::Stop() {
  if (!protocol_handler_)
    return;
  // Note that the call to Stop() below takes care of |protocol_handler_|
  // deletion.
  protocol_handler_->Stop();
  protocol_handler_ = NULL;
}

bool DevToolsServer::IsStarted() const {
  return protocol_handler_;
}
