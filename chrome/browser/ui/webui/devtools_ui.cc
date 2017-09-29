// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/user_agent.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/public/public_features.h"

using content::BrowserThread;
using content::WebContents;

namespace {

std::string PathWithoutParams(const std::string& path) {
  return GURL(std::string("chrome-devtools://devtools/") + path)
      .path().substr(1);
}

const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";

// DevToolsDataSource ---------------------------------------------------------

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

// An URLDataSource implementation that handles chrome-devtools://devtools/
// requests. Three types of requests could be handled based on the URL path:
// 1. /bundled/: bundled DevTools frontend is served.
// 2. /remote/: remote DevTools frontend is served from App Engine.
class DevToolsDataSource : public content::URLDataSource,
                           public net::URLFetcherDelegate {
 public:
  using GotDataCallback = content::URLDataSource::GotDataCallback;

  explicit DevToolsDataSource(net::URLRequestContextGetter* request_context);

  // content::URLDataSource implementation.
  std::string GetSource() const override;

  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const GotDataCallback& callback) override;

 private:
  // content::URLDataSource overrides.
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldAddContentSecurityPolicy() const override;
  bool ShouldDenyXFrameOptions() const override;
  bool ShouldServeMimeTypeAsContentTypeHeader() const override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Serves bundled DevTools frontend from ResourceBundle.
  void StartBundledDataRequest(const std::string& path,
                               const GotDataCallback& callback);

  // Serves remote DevTools frontend from hard-coded App Engine domain.
  void StartRemoteDataRequest(const std::string& path,
                              const GotDataCallback& callback);

  // Serves remote DevTools frontend from any endpoint, passed through
  // command-line flag.
  void StartCustomDataRequest(const GURL& url,
                              const GotDataCallback& callback);

  ~DevToolsDataSource() override;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  using PendingRequestsMap = std::map<const net::URLFetcher*, GotDataCallback>;
  PendingRequestsMap pending_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDataSource);
};

DevToolsDataSource::DevToolsDataSource(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

DevToolsDataSource::~DevToolsDataSource() {
  for (const auto& pair : pending_) {
    delete pair.first;
    pair.second.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
  }
}

std::string DevToolsDataSource::GetSource() const {
  return chrome::kChromeUIDevToolsHost;
}

void DevToolsDataSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  // Serve request from local bundle.
  std::string bundled_path_prefix(chrome::kChromeUIDevToolsBundledPath);
  bundled_path_prefix += "/";
  if (base::StartsWith(path, bundled_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    StartBundledDataRequest(path.substr(bundled_path_prefix.length()),
                            callback);
    return;
  }

  // Serve empty page.
  std::string empty_path_prefix(chrome::kChromeUIDevToolsBlankPath);
  if (base::StartsWith(path, empty_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    callback.Run(new base::RefCountedStaticMemory());
    return;
  }

  // Serve request from remote location.
  std::string remote_path_prefix(chrome::kChromeUIDevToolsRemotePath);
  remote_path_prefix += "/";
  if (base::StartsWith(path, remote_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    StartRemoteDataRequest(path.substr(remote_path_prefix.length()),
                           callback);
    return;
  }

  std::string custom_frontend_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCustomDevtoolsFrontend);

  if (custom_frontend_url.empty()) {
    callback.Run(NULL);
    return;
  }

  // Serve request from custom location.
  std::string custom_path_prefix(chrome::kChromeUIDevToolsCustomPath);
  custom_path_prefix += "/";

  if (base::StartsWith(path, custom_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    GURL url = GURL(custom_frontend_url +
                    path.substr(custom_path_prefix.length()));
    StartCustomDataRequest(url, callback);
    return;
  }

  callback.Run(NULL);
}

std::string DevToolsDataSource::GetMimeType(const std::string& path) const {
  return GetMimeTypeForPath(path);
}

bool DevToolsDataSource::ShouldAddContentSecurityPolicy() const {
  return false;
}

bool DevToolsDataSource::ShouldDenyXFrameOptions() const {
  return false;
}

bool DevToolsDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return true;
}

void DevToolsDataSource::StartBundledDataRequest(
    const std::string& path,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string filename = PathWithoutParams(path);
  base::StringPiece resource =
      content::DevToolsFrontendHost::GetFrontendResource(filename);

  DLOG_IF(WARNING, resource.empty())
      << "Unable to find dev tool resource: " << filename
      << ". If you compiled with debug_devtools=1, try running with "
         "--debug-devtools.";
  scoped_refptr<base::RefCountedStaticMemory> bytes(
      new base::RefCountedStaticMemory(resource.data(), resource.length()));
  callback.Run(bytes.get());
}

void DevToolsDataSource::StartRemoteDataRequest(
    const std::string& path,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url = GURL(kRemoteFrontendBase + path);
  CHECK_EQ(url.host(), kRemoteFrontendDomain);
  if (!url.is_valid()) {
    callback.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
    return;
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_hard_coded_data_source", R"(
        semantics {
          sender: "Developer Tools Remote Data Request From Google"
          description:
            "This service fetches Chromium DevTools front-end files from the "
            "cloud for the remote debugging scenario."
          trigger:
            "When user attaches to mobile phone for debugging."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DeveloperToolsDisabled {
              policy_options {mode: MANDATORY}
              DeveloperToolsDisabled: true
            }
          }
        })");
  net::URLFetcher* fetcher = net::URLFetcher::Create(url, net::URLFetcher::GET,
                                                     this, traffic_annotation)
                                 .release();
  pending_[fetcher] = callback;
  fetcher->SetRequestContext(request_context_.get());
  fetcher->Start();
}

void DevToolsDataSource::StartCustomDataRequest(
    const GURL& url,
    const content::URLDataSource::GotDataCallback& callback) {
  if (!url.is_valid()) {
    callback.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
    return;
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_free_data_source", R"(
        semantics {
          sender: "Developer Tools Remote Data Request"
          description:
            "This service fetches Chromium DevTools front-end files from the "
            "cloud for the remote debugging scenario. This can only happen if "
            "a URL was passed on the commandline via flag "
            "'--custom-devtools-frontend'. This URL overrides the default "
            "fetching from a Google website, see "
            "devtools_hard_coded_data_source."
          trigger:
            "When command line flag --custom-devtools-frontend is specified "
            "and DevTools is opened."
          data: "None"
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DeveloperToolsDisabled {
              policy_options {mode: MANDATORY}
              DeveloperToolsDisabled: true
            }
          }
        })");
  net::URLFetcher* fetcher = net::URLFetcher::Create(url, net::URLFetcher::GET,
                                                     this, traffic_annotation)
                                 .release();
  pending_[fetcher] = callback;
  fetcher->SetRequestContext(request_context_.get());
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->Start();
}

void DevToolsDataSource::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_.find(source);
  DCHECK(it != pending_.end());
  std::string response;
  source->GetResponseAsString(&response);
  delete source;
  it->second.Run(base::RefCountedString::TakeString(&response));
  pending_.erase(it);
}

}  // namespace

// DevToolsUI -----------------------------------------------------------------

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  if (url.scheme() == content::kChromeDevToolsScheme &&
      url.host() == chrome::kChromeUIDevToolsHost)
    return GURL();
  if (!url.is_valid() || url.host() != kRemoteFrontendDomain)
    return GURL();
  return GURL(base::StringPrintf("%s://%s/%s/%s",
              content::kChromeDevToolsScheme,
              chrome::kChromeUIDevToolsHost,
              chrome::kChromeUIDevToolsRemotePath,
              url.path().substr(1).c_str()));
}

// static
GURL DevToolsUI::GetRemoteBaseURL() {
  return GURL(base::StringPrintf(
      "%s%s/%s/",
      kRemoteFrontendBase,
      kRemoteFrontendPath,
      content::GetWebKitRevision().c_str()));
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), bindings_(web_ui->GetWebContents()) {
  web_ui->SetBindings(0);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile,
      new DevToolsDataSource(profile->GetRequestContext()));
}

DevToolsUI::~DevToolsUI() {
}
