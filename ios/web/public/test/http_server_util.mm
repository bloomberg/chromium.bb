// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/http_server_util.h"

#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/response_providers/file_based_response_provider.h"
#import "ios/web/public/test/response_providers/html_response_provider.h"

namespace web {
namespace test {

void SetUpSimpleHttpServer(const std::map<GURL, std::string>& responses) {
  SetUpHttpServer(base::MakeUnique<HtmlResponseProvider>(responses));
}

void SetUpSimpleHttpServerWithSetCookies(
    const std::map<GURL, std::pair<std::string, std::string>>& responses) {
  SetUpHttpServer(base::MakeUnique<HtmlResponseProvider>(responses));
}

void SetUpFileBasedHttpServer() {
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  SetUpHttpServer(base::MakeUnique<FileBasedResponseProvider>(path));
}

void SetUpHttpServer(std::unique_ptr<web::ResponseProvider> provider) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());

  server.RemoveAllResponseProviders();
  server.AddResponseProvider(std::move(provider));
}

void AddResponseProvider(std::unique_ptr<web::ResponseProvider> provider) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  server.AddResponseProvider(std::move(provider));
}

}  // namespace test
}  // namespace web
