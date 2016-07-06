// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/http_server_util.h"

#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/response_providers/file_based_response_provider.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"

namespace web {
namespace test {

void SetUpSimpleHttpServer(const std::map<GURL, std::string>& responses) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  auto provider = base::MakeUnique<HtmlResponseProvider>(responses);

  server.RemoveAllResponseProviders();
  server.AddResponseProvider(std::move(provider));
}

void SetUpFileBasedHttpServer() {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  auto provider = base::MakeUnique<FileBasedResponseProvider>(path);

  server.RemoveAllResponseProviders();
  server.AddResponseProvider(std::move(provider));
}

}  // namespace test
}  // namespace web
