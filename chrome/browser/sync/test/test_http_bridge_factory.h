// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_TEST_HTTP_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_SYNC_TEST_TEST_HTTP_BRIDGE_FACTORY_H_

#include "base/compiler_specific.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"

namespace browser_sync {

class TestHttpBridge : public syncer::HttpPostProviderInterface {
 public:
  // Begin syncer::HttpPostProviderInterface implementation:
  virtual void SetExtraRequestHeaders(const char * headers) OVERRIDE {}

  virtual void SetURL(const char* url, int port) OVERRIDE {}

  virtual void SetPostPayload(const char* content_type,
                              int content_length,
                              const char* content) OVERRIDE {}

  virtual bool MakeSynchronousPost(int* error_code,
                                   int* response_code) OVERRIDE;

  virtual int GetResponseContentLength() const OVERRIDE;

  virtual const char* GetResponseContent() const OVERRIDE;

  virtual const std::string GetResponseHeaderValue(
      const std::string&) const OVERRIDE;

  virtual void Abort() OVERRIDE;
  // End syncer::HttpPostProviderInterface implementation.
};

class TestHttpBridgeFactory : public syncer::HttpPostProviderFactory {
 public:
  TestHttpBridgeFactory();
  virtual ~TestHttpBridgeFactory();

  // syncer::HttpPostProviderFactory:
  virtual void Init(const std::string& user_agent) OVERRIDE;
  virtual syncer::HttpPostProviderInterface* Create() OVERRIDE;
  virtual void Destroy(syncer::HttpPostProviderInterface* http) OVERRIDE;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_TEST_HTTP_BRIDGE_FACTORY_H_
