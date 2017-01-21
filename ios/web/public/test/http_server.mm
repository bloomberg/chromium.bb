// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Core/GCDWebServer.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Core/GCDWebServerResponse.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Requests/GCDWebServerDataRequest.h"
#import "net/base/mac/url_conversions.h"

#include "url/gurl.h"

namespace {

// The default port on which the GCDWebServer is brought up on.
const NSUInteger kDefaultPort = 8080;

// Converts a GCDWebServerDataRequest (received from the GCDWebServer servlet)
// to a request object that the ResponseProvider expects.
web::ResponseProvider::Request ResponseProviderRequestFromGCDWebServerRequest(
    GCDWebServerDataRequest* request) {
  GURL url(net::GURLWithNSURL(request.URL));
  std::string method(base::SysNSStringToUTF8(request.method));
  base::scoped_nsobject<NSString> body(
      [[NSString alloc] initWithData:request.data
                            encoding:NSUTF8StringEncoding]);
  __block net::HttpRequestHeaders headers;
  [[request headers] enumerateKeysAndObjectsUsingBlock:^(NSString* header_key,
                                                         NSString* header_value,
                                                         BOOL*) {
      headers.SetHeader(base::SysNSStringToUTF8(header_key),
                        base::SysNSStringToUTF8(header_value));
  }];
  return web::ResponseProvider::Request(url, method,
                                        base::SysNSStringToUTF8(body), headers);
}

}  // namespace

namespace web {
namespace test {

RefCountedResponseProviderWrapper::RefCountedResponseProviderWrapper(
    std::unique_ptr<ResponseProvider> response_provider)
    : response_provider_(std::move(response_provider)) {}

RefCountedResponseProviderWrapper::~RefCountedResponseProviderWrapper() {}

// static
HttpServer& HttpServer::GetSharedInstance() {
  static web::test::HttpServer* shared_instance = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
      shared_instance = new HttpServer();
  });
  return *shared_instance;
}

// static
HttpServer& HttpServer::GetSharedInstanceWithResponseProviders(
    ProviderList response_providers) {
  DCHECK([NSThread isMainThread]);
  HttpServer& server = HttpServer::GetSharedInstance();
  // Use non-const reference as the response_provider ownership is transfered.
  for (std::unique_ptr<ResponseProvider>& provider : response_providers)
    server.AddResponseProvider(std::move(provider));
  return server;
}

void HttpServer::InitHttpServer() {
  DCHECK(gcd_web_server_);
  // Note: This block is called from an arbitrary GCD thread.
  id process_request =
      ^GCDWebServerResponse*(GCDWebServerDataRequest* request) {
      ResponseProvider::Request provider_request =
          ResponseProviderRequestFromGCDWebServerRequest(request);
      scoped_refptr<RefCountedResponseProviderWrapper>
          ref_counted_response_provider = GetResponseProviderForRequest(
              provider_request);

      if (!ref_counted_response_provider) {
        return [GCDWebServerResponse response];
      }
      ResponseProvider* response_provider =
          ref_counted_response_provider->GetResponseProvider();
      if (!response_provider) {
        return [GCDWebServerResponse response];
      }

      return response_provider->GetGCDWebServerResponse(provider_request);
  };
  [gcd_web_server_ removeAllHandlers];
  // Register a servlet for all HTTP GET, POST methods.
  [gcd_web_server_ addDefaultHandlerForMethod:@"GET"
                                 requestClass:[GCDWebServerDataRequest class]
                                 processBlock:process_request];
  [gcd_web_server_ addDefaultHandlerForMethod:@"POST"
                                 requestClass:[GCDWebServerDataRequest class]
                                 processBlock:process_request];
}

HttpServer::HttpServer() : port_(0) {
  gcd_web_server_.reset([[GCDWebServer alloc] init]);
  InitHttpServer();
}

HttpServer::~HttpServer() {
}

bool HttpServer::StartOnPort(NSUInteger port) {
  DCHECK([NSThread isMainThread]);
  DCHECK(!IsRunning()) << "The server is already running."
                       << " Please stop it before starting it again.";
  BOOL success = [gcd_web_server_ startWithPort:port bonjourName:@""];
  if (success) {
    SetPort(port);
  }
  return success;
}

void HttpServer::StartOrDie() {
  DCHECK([NSThread isMainThread]);
  StartOnPort(kDefaultPort);
  CHECK(IsRunning());
}

void HttpServer::Stop() {
  DCHECK([NSThread isMainThread]);
  DCHECK(IsRunning()) << "Cannot stop an already stopped server.";
  RemoveAllResponseProviders();
  [gcd_web_server_ stop];
  SetPort(0);
}

bool HttpServer::IsRunning() const {
  DCHECK([NSThread isMainThread]);
  return [gcd_web_server_ isRunning];
}

NSUInteger HttpServer::GetPort() const {
  base::AutoLock autolock(port_lock_);
  return port_;
}

// static
GURL HttpServer::MakeUrl(const std::string &url) {
  return HttpServer::GetSharedInstance().MakeUrlForHttpServer(url);
}

GURL HttpServer::MakeUrlForHttpServer(const std::string& url) const {
  GURL result(url);
  DCHECK(result.is_valid());
  const std::string kLocalhostHost = std::string("localhost");
  if (result.port() == base::IntToString(GetPort()) &&
      result.host() == kLocalhostHost) {
    return result;
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(kLocalhostHost);

  const std::string port = std::string(
      base::IntToString(static_cast<int>(GetPort())));
  replacements.SetPortStr(port);

  // It is necessary to prepend the host of the input URL so that URLs such
  // as http://origin/foo, http://destination/foo can be disamgiguated.
  const std::string new_path = std::string(result.host() + result.path());
  replacements.SetPathStr(new_path);

  return result.ReplaceComponents(replacements);
}

scoped_refptr<RefCountedResponseProviderWrapper>
    HttpServer::GetResponseProviderForRequest(
        const web::ResponseProvider::Request& request) {
  base::AutoLock autolock(provider_list_lock_);
  scoped_refptr<RefCountedResponseProviderWrapper> result;
  for (const auto& ref_counted_response_provider : providers_) {
    ResponseProvider* response_provider =
        ref_counted_response_provider.get()->GetResponseProvider();
    if (response_provider->CanHandleRequest(request)) {
      DCHECK(!result) <<
          "No more than one response provider can handle the same request.";
      result = ref_counted_response_provider;
    }
  }
  return result;
}

void HttpServer::AddResponseProvider(
    std::unique_ptr<ResponseProvider> response_provider) {
  DCHECK([NSThread isMainThread]);
  DCHECK(IsRunning()) << "Can add a response provider only when the server is "
                      << "running.";
  base::AutoLock autolock(provider_list_lock_);
  scoped_refptr<RefCountedResponseProviderWrapper>
      ref_counted_response_provider(
          new RefCountedResponseProviderWrapper(std::move(response_provider)));
  providers_.push_back(ref_counted_response_provider);
}

void HttpServer::RemoveResponseProvider(ResponseProvider* response_provider) {
  DCHECK([NSThread isMainThread]);
  base::AutoLock autolock(provider_list_lock_);
  auto found_iter = providers_.end();
  for (auto it = providers_.begin(); it != providers_.end(); ++it) {
    if ((*it)->GetResponseProvider() == response_provider) {
      found_iter = it;
      break;
    }
  }
  if (found_iter != providers_.end()) {
    providers_.erase(found_iter);
  }
}

void HttpServer::RemoveAllResponseProviders() {
  DCHECK([NSThread isMainThread]);
  base::AutoLock autolock(provider_list_lock_);
  providers_.clear();
}

void HttpServer::SetPort(NSUInteger port) {
  base::AutoLock autolock(port_lock_);
  port_ = port;
}

} // namespace test
} // namespace web
