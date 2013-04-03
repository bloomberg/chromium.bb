// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/net/chrome_url_request_context.h"
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

class FetchRequest : public net::URLFetcherDelegate {
 public:
  FetchRequest(net::URLRequestContextGetter* request_context,
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

 private:
  virtual ~FetchRequest() {}

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    std::string response;
    if (source)
      source->GetResponseAsString(&response);
    else
      response = kHttpNotFound;

    callback_.Run(base::RefCountedString::TakeString(&response));
    delete this;
  }

  scoped_ptr<net::URLFetcher> fetcher_;
  content::URLDataSource::GotDataCallback callback_;
};

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

class BundledDataSource : public content::URLDataSource {
 public:
  explicit BundledDataSource() {
  }

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE {
    return chrome::kChromeUIDevToolsBundledHost;
  }

  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE {
    std::string filename = PathWithoutParams(path);

    int resource_id =
        content::DevToolsHttpHandler::GetFrontendResourceId(filename);

    DLOG_IF(WARNING, -1 == resource_id) << "Unable to find dev tool resource: "
        << filename << ". If you compiled with debug_devtools=1, try running"
        " with --debug-devtools.";
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    scoped_refptr<base::RefCountedStaticMemory> bytes(rb.LoadDataResourceBytes(
        resource_id));
    callback.Run(bytes);
  }

  virtual std::string GetMimeType(const std::string& path) const OVERRIDE {
    return GetMimeTypeForPath(path);
  }

  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 private:
  virtual ~BundledDataSource() {}
  DISALLOW_COPY_AND_ASSIGN(BundledDataSource);
};

class RemoteDataSource : public content::URLDataSource {
 public:
  explicit RemoteDataSource(net::URLRequestContextGetter*
      request_context) : request_context_(request_context) {
  }

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE {
    return chrome::kChromeUIDevToolsRemoteHost;
  }

  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE {

    GURL url = GURL(kRemoteFrontendBase + path);
    CHECK_EQ(url.host(), kRemoteFrontendDomain);
    new FetchRequest(request_context_, url, callback);
  }

  virtual std::string GetMimeType(const std::string& path) const OVERRIDE {
    return GetMimeTypeForPath(path);
  }

  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 private:
  virtual ~RemoteDataSource() {}
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDataSource);
};

}  // namespace

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  CHECK(url.is_valid());
  CHECK_EQ(url.host(), kRemoteFrontendDomain);
  return GURL(base::StringPrintf("%s://%s/%s", chrome::kChromeDevToolsScheme,
              chrome::kChromeUIDevToolsRemoteHost,
              url.path().substr(1).c_str()));
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->SetBindings(0);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile,
      new BundledDataSource());
  content::URLDataSource::Add(
      profile,
      new RemoteDataSource(profile->GetRequestContext()));
}
