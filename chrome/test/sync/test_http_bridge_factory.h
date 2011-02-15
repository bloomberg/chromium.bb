// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SYNC_TEST_HTTP_BRIDGE_FACTORY_H_
#define CHROME_TEST_SYNC_TEST_HTTP_BRIDGE_FACTORY_H_
#pragma once

#include "chrome/browser/sync/engine/syncapi.h"

namespace browser_sync {

class TestHttpBridge : public sync_api::HttpPostProviderInterface {
 public:
  virtual void SetUserAgent(const char* user_agent) {}

  // Add additional headers to the request.
  virtual void SetExtraRequestHeaders(const char * headers) {}

  // Set the URL to POST to.
  virtual void SetURL(const char* url, int port) {}

  // Set the type, length and content of the POST payload.
  // |content_type| is a null-terminated MIME type specifier.
  // |content| is a data buffer; Do not interpret as a null-terminated string.
  // |content_length| is the total number of chars in |content|. It is used to
  // assign/copy |content| data.
  virtual void SetPostPayload(const char* content_type, int content_length,
      const char* content) {}

  // Returns true if the URL request succeeded. If the request failed,
  // os_error() may be non-zero and hence contain more information.
  virtual bool MakeSynchronousPost(int* os_error_code, int* response_code);

  // Get the length of the content returned in the HTTP response.
  // This does not count the trailing null-terminating character returned
  // by GetResponseContent, so it is analogous to calling string.length.
  virtual int GetResponseContentLength() const;

  // Get the content returned in the HTTP response.
  // This is a null terminated string of characters.
  // Value should be copied.
  virtual const char* GetResponseContent() const;

  virtual const std::string GetResponseHeaderValue(const std::string&) const;
};

class TestHttpBridgeFactory : public sync_api::HttpPostProviderFactory {
 public:
  // Override everything to do nothing.
  TestHttpBridgeFactory();
  virtual ~TestHttpBridgeFactory();

  // sync_api::HttpPostProviderFactory:
  virtual sync_api::HttpPostProviderInterface* Create();
  virtual void Destroy(sync_api::HttpPostProviderInterface* http);
};

}  // namespace browser_sync

#endif  // CHROME_TEST_SYNC_TEST_HTTP_BRIDGE_FACTORY_H_
