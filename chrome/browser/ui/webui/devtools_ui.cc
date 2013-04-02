// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
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

const char kHostedFrontendDomain[] = "chrome-devtools-frontend.appspot.com";
const char kHostedFrontendBase[] =
    "https://chrome-devtools-frontend.appspot.com/";
const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";

class FetchRequest : public net::URLFetcherDelegate {
 public:
  FetchRequest(net::URLRequestContextGetter* request_context,
               const GURL& url,
               const content::URLDataSource::GotDataCallback& callback)
      : callback_(callback) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    bool hosted_frontend =
        command_line.HasSwitch(switches::kEnableHostedDevToolsFrontend);
    if (!hosted_frontend || !url.is_valid()) {
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

}  // namespace

class DevToolsDataSource : public content::URLDataSource {
 public:
  explicit DevToolsDataSource(net::URLRequestContextGetter* request_context);

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE;

 private:
  virtual ~DevToolsDataSource() {}
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsDataSource);
};


DevToolsDataSource::DevToolsDataSource(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

std::string DevToolsDataSource::GetSource() {
  return chrome::kChromeUIDevToolsHost;
}

void DevToolsDataSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string filename = PathWithoutParams(path);

  if (filename.find(chrome::kChromeUIDevToolsHostedPath) == 0) {
    GURL url = GURL(kHostedFrontendBase +
        filename.substr(strlen(chrome::kChromeUIDevToolsHostedPath)));
    CHECK(url.host() == kHostedFrontendDomain);
    new FetchRequest(request_context_, url, callback);
    return;
  }

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

std::string DevToolsDataSource::GetMimeType(const std::string& path) const {
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

bool DevToolsDataSource::ShouldAddContentSecurityPolicy() const {
  return false;
}

// static
void DevToolsUI::RegisterDevToolsDataSource(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static bool registered = false;
  if (!registered) {
    content::URLDataSource::Add(profile, new DevToolsDataSource(
        profile->GetRequestContext()));
    registered = true;
  }
}

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  CHECK(url.is_valid());
  CHECK_EQ(url.host(), kHostedFrontendDomain);
  return GURL(base::StringPrintf("%s://%s/%s%s", chrome::kChromeDevToolsScheme,
              chrome::kChromeUIDevToolsHost,
              chrome::kChromeUIDevToolsHostedPath,
              url.path().substr(1).c_str()));
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->SetBindings(0);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile,
      new DevToolsDataSource(profile->GetRequestContext()));
}
