// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/launcher/launcher.mojom.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "url/gurl.h"

namespace mojo {

namespace {

typedef mojo::Callback<void(String handler_url, String view_url,
    ResponseDetailsPtr response)> LaunchCallback;

}

class LauncherApp;

class LauncherConnection : public InterfaceImpl<Launcher> {
 public:
  explicit LauncherConnection(LauncherApp* app) : app_(app) {}
  virtual ~LauncherConnection() {}

 private:
  // Overridden from Launcher:
  virtual void Launch(const String& url,
                      const LaunchCallback& callback) OVERRIDE;

  LauncherApp* app_;

  DISALLOW_COPY_AND_ASSIGN(LauncherConnection);
};

class LaunchInstance {
 public:
  LaunchInstance(LauncherApp* app,
                 const LaunchCallback& callback,
                 const String& url);
  virtual ~LaunchInstance() {}

 private:
  void OnReceivedResponse(URLResponsePtr response);

  std::string GetContentType(const Array<String>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
      base::StringTokenizer t(headers[i], ": ;=");
      while (t.GetNext()) {
        if (!t.token_is_delim() &&
            LowerCaseEqualsASCII(t.token(), "content-type")) {
          while (t.GetNext()) {
            if (!t.token_is_delim())
              return t.token();
          }
        }
      }
    }
    return "";
  }

  void ScheduleDestroy() {
    if (destroy_scheduled_)
      return;
    destroy_scheduled_ = true;
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  LauncherApp* app_;
  bool destroy_scheduled_;
  const LaunchCallback callback_;
  URLLoaderPtr url_loader_;
  ScopedDataPipeConsumerHandle response_body_stream_;

  DISALLOW_COPY_AND_ASSIGN(LaunchInstance);
};

class LauncherApp : public ApplicationDelegate {
 public:
  LauncherApp() : launcher_connection_factory_(this) {
    handler_map_["text/html"] = "mojo:mojo_html_viewer";
    handler_map_["image/png"] = "mojo:mojo_media_viewer";
  }
  virtual ~LauncherApp() {}

  URLLoaderPtr CreateURLLoader() {
    URLLoaderPtr loader;
    network_service_->CreateURLLoader(Get(&loader));
    return loader.Pass();
  }

  std::string GetHandlerForContentType(const std::string& content_type) {
    HandlerMap::const_iterator it = handler_map_.find(content_type);
    return it != handler_map_.end() ? it->second : "";
  }

 private:
  typedef std::map<std::string, std::string> HandlerMap;

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_network_service", &network_service_);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&launcher_connection_factory_);
    return true;
  }

  InterfaceFactoryImplWithContext<LauncherConnection, LauncherApp>
      launcher_connection_factory_;

  HandlerMap handler_map_;
  NetworkServicePtr network_service_;

  DISALLOW_COPY_AND_ASSIGN(LauncherApp);
};

void LauncherConnection::Launch(const String& url_string,
                                const LaunchCallback& callback) {
  GURL url(url_string.To<std::string>());

  // For Mojo URLs, the handler can always be found at the origin.
  // TODO(aa): Return error for invalid URL?
  if (url.is_valid() && url.SchemeIs("mojo")) {
    callback.Run(url.GetOrigin().spec(), url_string, ResponseDetailsPtr());
    return;
  }
  new LaunchInstance(app_, callback, url_string);
}

LaunchInstance::LaunchInstance(LauncherApp* app,
                               const LaunchCallback& callback,
                               const String& url)
    : app_(app),
      destroy_scheduled_(false),
      callback_(callback) {
  URLRequestPtr request(URLRequest::New());
  request->url = url;
  request->method = "GET";
  request->auto_follow_redirects = true;

  url_loader_ = app_->CreateURLLoader();
  url_loader_->Start(request.Pass(),
                     base::Bind(&LaunchInstance::OnReceivedResponse,
                                base::Unretained(this)));
}

void LaunchInstance::OnReceivedResponse(URLResponsePtr response) {
  if (!response->error) {
    std::string content_type = GetContentType(response->headers);
    std::string handler_url = app_->GetHandlerForContentType(content_type);
    if (handler_url.empty()) {
      DLOG(WARNING) << "No handler for content type: " << content_type;
    } else {
      ResponseDetailsPtr nav_response(ResponseDetails::New());
      nav_response->loader = url_loader_.Pass();
      nav_response->response = response.Pass();
      String response_url = nav_response->response->url;
      callback_.Run(handler_url, response_url, nav_response.Pass());
    }
  }
  ScheduleDestroy();
}

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new LauncherApp;
}

}  // namespace mojo
