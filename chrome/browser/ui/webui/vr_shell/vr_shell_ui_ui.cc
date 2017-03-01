// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_ui.h"

#include <string>
#include <unordered_set>

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"

#if !defined(ENABLE_VR_SHELL_UI_DEV)
#include "chrome/browser/browser_process.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#else
#include <map>
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "content/public/browser/url_data_source.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#endif

namespace {

#if defined(ENABLE_VR_SHELL_UI_DEV)
std::string PathWithoutParams(const std::string& path) {
  return GURL(std::string("chrome://vr-shell-ui/") + path).path().substr(1);
}

const char kRemoteBase[] = "http://localhost:8080/";
const char kRemoteBaseAlt[] = "https://jcarpenter.github.io/hoverboard-ui/";
const char kRemoteDefaultPath[] = "vr_shell_ui.html";
const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";

// RemoteDataSource ---------------------------------------------------------

std::string GetMimeTypeForPath(const std::string& path) {
  std::string filename = PathWithoutParams(path);
  if (base::EndsWith(filename, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/html";
  } else if (base::EndsWith(filename, ".css",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/css";
  } else if (base::EndsWith(filename, ".js",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/javascript";
  } else if (base::EndsWith(filename, ".png",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/png";
  } else if (base::EndsWith(filename, ".gif",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/gif";
  } else if (base::EndsWith(filename, ".svg",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/svg+xml";
  } else if (base::EndsWith(filename, ".manifest",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/cache-manifest";
  }
  return "text/html";
}

class RemoteDataSource : public content::URLDataSource,
                         public net::URLFetcherDelegate {
 public:
  using GotDataCallback = content::URLDataSource::GotDataCallback;

  explicit RemoteDataSource(net::URLRequestContextGetter* request_context);

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const GotDataCallback& callback) override;

  bool AllowCaching() const override { return false; }

 private:
  // content::URLDataSource overrides.
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldAddContentSecurityPolicy() const override;
  bool ShouldDenyXFrameOptions() const override;
  bool ShouldServeMimeTypeAsContentTypeHeader() const override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  ~RemoteDataSource() override;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  using PendingRequestsMap = std::map<const net::URLFetcher*, GotDataCallback>;
  PendingRequestsMap pending_;
  bool use_localhost_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDataSource);
};

RemoteDataSource::RemoteDataSource(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context), use_localhost_(true) {}

RemoteDataSource::~RemoteDataSource() {
  for (const auto& pair : pending_) {
    delete pair.first;
    pair.second.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
  }
}

std::string RemoteDataSource::GetSource() const {
  return chrome::kChromeUIVrShellUIHost;
}

void RemoteDataSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url = GURL((use_localhost_ ? kRemoteBase : kRemoteBaseAlt) +
                  (path.empty() ? std::string(kRemoteDefaultPath) : path));
  if (!url.is_valid()) {
    callback.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
    return;
  }
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, this).release();

  fetcher->AddExtraRequestHeader("Cache-Control: no-cache");

  pending_[fetcher] = callback;
  fetcher->SetRequestContext(request_context_.get());
  fetcher->Start();
}

std::string RemoteDataSource::GetMimeType(const std::string& path) const {
  return GetMimeTypeForPath(path);
}

bool RemoteDataSource::ShouldAddContentSecurityPolicy() const {
  return false;
}

bool RemoteDataSource::ShouldDenyXFrameOptions() const {
  return false;
}

bool RemoteDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return true;
}

void RemoteDataSource::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_.find(source);
  DCHECK(it != pending_.end());
  std::string response;
  source->GetResponseAsString(&response);

  if (response.empty() && use_localhost_) {
    if (source->GetOriginalURL().path().substr(1) == kRemoteDefaultPath) {
      // Failed to request default page from local host, try request default
      // page from remote server. Empty string indicates default page.
      use_localhost_ = false;
      content::URLDataSource::GotDataCallback callback = it->second;
      StartDataRequest(std::string(),
                       content::ResourceRequestInfo::WebContentsGetter(),
                       callback);
    }
  } else {
    it->second.Run(base::RefCountedString::TakeString(&response));
  }
  delete source;
  pending_.erase(it);
}
#else
content::WebUIDataSource* CreateVrShellUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIVrShellUIHost);
  source->UseGzip(std::unordered_set<std::string>() /* excluded_paths */);
  source->AddResourcePath("vr_shell_ui.css", IDR_VR_SHELL_UI_CSS);
  source->AddResourcePath("vr_shell_ui.js", IDR_VR_SHELL_UI_JS);
  source->AddResourcePath("vr_shell_ui_api.js", IDR_VR_SHELL_UI_API_JS);
  source->AddResourcePath("vr_shell_ui_scene.js", IDR_VR_SHELL_UI_SCENE_JS);
  source->AddResourcePath("vk.css", IDR_VR_SHELL_UI_VK_CSS);
  source->AddResourcePath("vk.js", IDR_VR_SHELL_UI_VK_JS);
  source->SetDefaultResource(IDR_VR_SHELL_UI_HTML);
  source->AddLocalizedString(
      "insecureWebVrContentPermanent",
      IDS_WEBSITE_SETTINGS_INSECURE_WEBVR_CONTENT_PERMANENT);
  source->AddLocalizedString(
      "insecureWebVrContentTransient",
      IDS_WEBSITE_SETTINGS_INSECURE_WEBVR_CONTENT_TRANSIENT);
  source->AddLocalizedString("back", IDS_VR_SHELL_UI_BACK_BUTTON);
  source->AddLocalizedString("forward", IDS_VR_SHELL_UI_FORWARD_BUTTON);
  source->AddLocalizedString("reload", IDS_VR_SHELL_UI_RELOAD_BUTTON);
  source->AddLocalizedString("newTab", IDS_VR_SHELL_NEW_TAB_BUTTON);
  source->AddLocalizedString("newIncognitoTab",
                             IDS_VR_SHELL_NEW_INCOGNITO_TAB_BUTTON);

  return source;
}
#endif

}  // namespace

VrShellUIUI::VrShellUIUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
#if !defined(ENABLE_VR_SHELL_UI_DEV)
  content::WebUIDataSource::Add(profile, CreateVrShellUIHTMLSource());
#else
  content::URLDataSource::Add(
      profile, new RemoteDataSource(profile->GetRequestContext()));
#endif
  web_ui->AddMessageHandler(base::MakeUnique<VrShellUIMessageHandler>());
}

VrShellUIUI::~VrShellUIUI() {}
