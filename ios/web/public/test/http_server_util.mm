// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/http_server_util.h"

#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"

namespace web {
namespace test {

void SetUpSimpleHttpServer(std::map<GURL, std::string> responses) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  std::unique_ptr<web::ResponseProvider> provider;
  provider.reset(new HtmlResponseProvider(responses));

  server.RemoveAllResponseProviders();
  server.AddResponseProvider(provider.release());
}

}  // namespace test
}  // namespace web
