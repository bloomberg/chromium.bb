// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

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
const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";

#if defined(DEBUG_DEVTOOLS)
// Local frontend url provided by InspectUI.
const char kFallbackFrontendURL[] =
    "chrome-devtools://devtools/bundled/devtools.html";
#else
// URL causing the DevTools window to display a plain text warning.
const char kFallbackFrontendURL[] =
    "data:text/plain,Cannot load DevTools frontend from an untrusted origin";
#endif  // defined(DEBUG_DEVTOOLS)

// FetchRequest ---------------------------------------------------------------

class FetchRequest : public net::URLFetcherDelegate {
 public:
  FetchRequest(net::URLRequestContextGetter* request_context,
               const GURL& url,
               const content::URLDataSource::GotDataCallback& callback);

 private:
  virtual ~FetchRequest() {}
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  scoped_ptr<net::URLFetcher> fetcher_;
  content::URLDataSource::GotDataCallback callback_;
};

FetchRequest::FetchRequest(
    net::URLRequestContextGetter* request_context,
    const GURL& url,
    const content::URLDataSource::GotDataCallback& callback)
    : callback_(callback) {
  if (!url.is_valid()) {
    OnURLFetchComplete(NULL);
    return;
  }

  fetcher_.reset(net::URLFetcher::Create(url, net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(request_context);
  fetcher_->Start();
}

void FetchRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string response;
  if (source)
    source->GetResponseAsString(&response);
  else
    response = kHttpNotFound;

  callback_.Run(base::RefCountedString::TakeString(&response));
  delete this;
}

// DevToolsDataSource ---------------------------------------------------------

std::string GetMimeTypeForPath(const std::string& path) {
  std::string filename = PathWithoutParams(path);
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  } else if (EndsWith(filename, ".manifest", false)) {
    return "text/cache-manifest";
  }
  NOTREACHED();
  return "text/plain";
}

// An URLDataSource implementation that handles chrome-devtools://devtools/
// requests. Three types of requests could be handled based on the URL path:
// 1. /bundled/: bundled DevTools frontend is served.
// 2. /remote/: Remote DevTools frontend is served from App Engine.
class DevToolsDataSource : public content::URLDataSource {
 public:
  explicit DevToolsDataSource(net::URLRequestContextGetter* request_context);

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;

  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;

 private:
  // content::URLDataSource overrides.
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE;
  virtual bool ShouldServeMimeTypeAsContentTypeHeader() const OVERRIDE;

  // Serves bundled DevTools frontend from ResourceBundle.
  void StartBundledDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback);

  // Serves remote DevTools frontend from hard-coded App Engine domain.
  void StartRemoteDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback);

  virtual ~DevToolsDataSource() {}
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDataSource);
};

DevToolsDataSource::DevToolsDataSource(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

std::string DevToolsDataSource::GetSource() const {
  return chrome::kChromeUIDevToolsHost;
}

void DevToolsDataSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // Serve request from local bundle.
  std::string bundled_path_prefix(chrome::kChromeUIDevToolsBundledPath);
  bundled_path_prefix += "/";
  if (StartsWithASCII(path, bundled_path_prefix, false)) {
    StartBundledDataRequest(path.substr(bundled_path_prefix.length()),
                            render_process_id, render_frame_id, callback);
    return;
  }

  // Serve request from remote location.
  std::string remote_path_prefix(chrome::kChromeUIDevToolsRemotePath);
  remote_path_prefix += "/";
  if (StartsWithASCII(path, remote_path_prefix, false)) {
    StartRemoteDataRequest(path.substr(remote_path_prefix.length()),
                           render_process_id, render_frame_id, callback);
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

bool DevToolsDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return true;
}

void DevToolsDataSource::StartBundledDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string filename = PathWithoutParams(path);

  int resource_id =
      content::DevToolsHttpHandler::GetFrontendResourceId(filename);

  DLOG_IF(WARNING, -1 == resource_id) << "Unable to find dev tool resource: "
      << filename << ". If you compiled with debug_devtools=1, try running"
      " with --debug-devtools.";
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<base::RefCountedStaticMemory> bytes(rb.LoadDataResourceBytes(
      resource_id));
  callback.Run(bytes.get());
}

void DevToolsDataSource::StartRemoteDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url = GURL(kRemoteFrontendBase + path);
  CHECK_EQ(url.host(), kRemoteFrontendDomain);
  new FetchRequest(request_context_, url, callback);
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

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->SetBindings(0);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile,
      new DevToolsDataSource(profile->GetRequestContext()));
}
