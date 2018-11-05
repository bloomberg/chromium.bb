// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file comes from Google Home(cast) implementation.

#ifndef CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_HTTP_CONNECTION_H_
#define CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_HTTP_CONNECTION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "libassistant/shared/internal_api/http_connection.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {
namespace assistant {

// Implements libassistant's HttpConnection using Chromium //net library.
class ChromiumHttpConnection
    : public assistant_client::HttpConnection,
      public ::net::URLFetcherDelegate,
      public base::RefCountedThreadSafe<ChromiumHttpConnection> {
 public:
  ChromiumHttpConnection(
      scoped_refptr<::net::URLRequestContextGetter> url_request_context_getter,
      Delegate* delegate);

  // assistant_client::HttpConnection implementation:
  void SetRequest(const std::string& url, Method method) override;
  void AddHeader(const std::string& name, const std::string& value) override;
  void SetUploadContent(const std::string& content,
                        const std::string& content_type) override;
  void SetChunkedUploadContentType(const std::string& content_type) override;
  void EnableHeaderResponse() override;
  void EnablePartialResults() override;
  void Start() override;
  void Pause() override;
  void Resume() override;
  void Close() override;
  void UploadData(const std::string& data, bool is_last_chunk) override;

 protected:
  ~ChromiumHttpConnection() override;

 private:
  friend class base::RefCountedThreadSafe<ChromiumHttpConnection>;

  enum class State {
    NEW,
    STARTED,
    COMPLETED,
    DESTROYED,
  };

  // HttpConnection methods, re-scheduled on network thread:
  void SetRequestOnThread(const std::string& url, Method method);
  void AddHeaderOnThread(const std::string& name, const std::string& value);
  void SetUploadContentOnThread(const std::string& content,
                                const std::string& content_type);
  void SetChunkedUploadContentTypeOnThread(const std::string& content_type);
  void EnablePartialResultsOnThread();
  void StartOnThread();
  void CloseOnThread();
  void UploadDataOnThread(const std::string& data, bool is_last_chunk);

  // URLFetcherDelegate implementation:
  void OnURLFetchComplete(const ::net::URLFetcher* source) override;

  scoped_refptr<::net::URLRequestContextGetter> url_request_context_getter_;
  Delegate* const delegate_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  State state_ = State::NEW;

  // Parameters to be set before Start() call.
  GURL url_;
  Method method_ = Method::GET;
  ::net::HttpRequestHeaders headers_;
  std::string upload_content_;
  std::string upload_content_type_;
  std::string chunked_upload_content_type_;
  bool handle_partial_response_ = false;

  // |url_fetcher_| has to be accessed by network thread only. Some methods
  // of URLFetcher (especially GetResponseAsString()) are not safe to access
  // from other threads even under lock, since chromium accesses it as well.
  std::unique_ptr<::net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumHttpConnection);
};

class ChromiumHttpConnectionFactory
    : public assistant_client::HttpConnectionFactory {
 public:
  ChromiumHttpConnectionFactory(
      scoped_refptr<::net::URLRequestContextGetter> url_request_context_getter);
  ~ChromiumHttpConnectionFactory() override;

  // assistant_client::HttpConnectionFactory implementation:
  assistant_client::HttpConnection* Create(
      assistant_client::HttpConnection::Delegate* delegate) override;

 private:
  scoped_refptr<::net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumHttpConnectionFactory);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_HTTP_CONNECTION_H_
