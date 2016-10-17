// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/user_agent.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::WebContents;

namespace {

std::string PathWithoutParams(const std::string& path) {
  return GURL(std::string("chrome-devtools://devtools/") + path)
      .path().substr(1);
}

const char kRemoteFrontendDomain[] = "chrome-devtools-frontend.appspot.com";
const char kRemoteFrontendBase[] =
    "https://chrome-devtools-frontend.appspot.com/";
const char kRemoteFrontendPath[] = "serve_file";
const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";

#if defined(DEBUG_DEVTOOLS)
// Local frontend url provided by InspectUI.
const char kFallbackFrontendURL[] =
    "chrome-devtools://devtools/bundled/inspector.html";
#else
// URL causing the DevTools window to display a plain text warning.
const char kFallbackFrontendURL[] =
    "data:text/plain,Cannot load DevTools frontend from an untrusted origin";
#endif  // defined(DEBUG_DEVTOOLS)

GURL SanitizeFrontendURL(
    const GURL& url,
    const std::string& scheme,
    const std::string& host,
    const std::string& path,
    bool allow_query);

std::string SanitizeRevision(const std::string& revision) {
  for (size_t i = 0; i < revision.length(); i++) {
    if (!(revision[i] == '@' && i == 0)
        && !(revision[i] >= '0' && revision[i] <= '9')
        && !(revision[i] >= 'a' && revision[i] <= 'z')
        && !(revision[i] >= 'A' && revision[i] <= 'Z')) {
      return std::string();
    }
  }
  return revision;
}

std::string SanitizeFrontendPath(const std::string& path) {
  for (size_t i = 0; i < path.length(); i++) {
    if (path[i] != '/' && path[i] != '-' && path[i] != '_'
        && path[i] != '.' && path[i] != '@'
        && !(path[i] >= '0' && path[i] <= '9')
        && !(path[i] >= 'a' && path[i] <= 'z')
        && !(path[i] >= 'A' && path[i] <= 'Z')) {
      return std::string();
    }
  }
  return path;
}

std::string SanitizeEndpoint(const std::string& value) {
  if (value.find('&') != std::string::npos
      || value.find('?') != std::string::npos)
    return std::string();
  return value;
}

std::string SanitizeRemoteBase(const std::string& value) {
  GURL url(value);
  std::string path = url.path();
  std::vector<std::string> parts = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  path = base::StringPrintf("/%s/%s/", kRemoteFrontendPath, revision.c_str());
  return SanitizeFrontendURL(url, url::kHttpsScheme,
                             kRemoteFrontendDomain, path, false).spec();
}

std::string SanitizeRemoteFrontendURL(const std::string& value) {
  GURL url(net::UnescapeURLComponent(value,
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
      net::UnescapeRule::REPLACE_PLUS_WITH_SPACE));
  std::string path = url.path();
  std::vector<std::string> parts = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  std::string filename = parts.size() ? parts[parts.size() - 1] : "";
  if (filename != "devtools.html")
    filename = "inspector.html";
  path = base::StringPrintf("/serve_rev/%s/%s",
                            revision.c_str(), filename.c_str());
  std::string sanitized = SanitizeFrontendURL(url, url::kHttpsScheme,
      kRemoteFrontendDomain, path, true).spec();
  return net::EscapeQueryParamValue(sanitized, false);
}

std::string SanitizeFrontendQueryParam(
    const std::string& key,
    const std::string& value) {
  // Convert boolean flags to true.
  if (key == "can_dock" || key == "debugFrontend" || key == "experiments" ||
      key == "isSharedWorker" || key == "v8only" || key == "remoteFrontend")
    return "true";

  // Pass connection endpoints as is.
  if (key == "ws" || key == "service-backend")
    return SanitizeEndpoint(value);

  // Only support undocked for old frontends.
  if (key == "dockSide" && value == "undocked")
    return value;

  if (key == "remoteBase")
    return SanitizeRemoteBase(value);

  if (key == "remoteFrontendUrl")
    return SanitizeRemoteFrontendURL(value);

  return std::string();
}

GURL SanitizeFrontendURL(
    const GURL& url,
    const std::string& scheme,
    const std::string& host,
    const std::string& path,
    bool allow_query) {
  std::vector<std::string> query_parts;
  if (allow_query) {
    for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
      std::string value = SanitizeFrontendQueryParam(it.GetKey(),
          it.GetValue());
      if (!value.empty()) {
        query_parts.push_back(
            base::StringPrintf("%s=%s", it.GetKey().c_str(), value.c_str()));
      }
    }
  }
  std::string query =
      query_parts.empty() ? "" : "?" + base::JoinString(query_parts, "&");
  std::string constructed = base::StringPrintf("%s://%s%s%s",
      scheme.c_str(), host.c_str(), path.c_str(), query.c_str());
  GURL result = GURL(constructed);
  if (!result.is_valid())
    return GURL();
  return result;
}

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

  // Serve request from remote location.
  std::string remote_path_prefix(chrome::kChromeUIDevToolsRemotePath);
  remote_path_prefix += "/";
  if (base::StartsWith(path, remote_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    StartRemoteDataRequest(path.substr(remote_path_prefix.length()),
                           callback);
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
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, this).release();
  pending_[fetcher] = callback;
  fetcher->SetRequestContext(request_context_.get());
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
  if (!url.is_valid() || url.host() != kRemoteFrontendDomain)
    return GURL(kFallbackFrontendURL);
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

// static
GURL DevToolsUI::SanitizeFrontendURL(const GURL& url) {
  return ::SanitizeFrontendURL(url, content::kChromeDevToolsScheme,
      chrome::kChromeUIDevToolsHost, SanitizeFrontendPath(url.path()), true);
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->SetBindings(0);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile,
      new DevToolsDataSource(profile->GetRequestContext()));

  GURL url = web_ui->GetWebContents()->GetVisibleURL();
  if (url.spec() == SanitizeFrontendURL(url).spec())
    bindings_.reset(new DevToolsUIBindings(web_ui->GetWebContents()));
}

DevToolsUI::~DevToolsUI() {
}
