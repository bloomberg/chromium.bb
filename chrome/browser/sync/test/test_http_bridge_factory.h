// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_TEST_HTTP_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_SYNC_TEST_TEST_HTTP_BRIDGE_FACTORY_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/sync/internal_api/http_post_provider_interface.h"
#include "chrome/browser/sync/internal_api/http_post_provider_factory.h"

namespace browser_sync {

class TestHttpBridge : public sync_api::HttpPostProviderInterface {
 public:
  // Begin sync_api::HttpPostProviderInterface implementation:
  virtual void SetUserAgent(const char* user_agent) OVERRIDE {}

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
  // End sync_api::HttpPostProviderInterface implementation.
};

class TestHttpBridgeFactory : public sync_api::HttpPostProviderFactory {
 public:
  TestHttpBridgeFactory();
  virtual ~TestHttpBridgeFactory();

  // sync_api::HttpPostProviderFactory:
  virtual sync_api::HttpPostProviderInterface* Create() OVERRIDE;
  virtual void Destroy(sync_api::HttpPostProviderInterface* http) OVERRIDE;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_TEST_HTTP_BRIDGE_FACTORY_H_
