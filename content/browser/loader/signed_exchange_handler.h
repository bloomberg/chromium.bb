// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "mojo/common/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace mojo {
class StringDataPipeProducer;
}  // namespace mojo

namespace network {
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

// IMPORTANT: Currenly SignedExchangeHandler doesn't implement any CBOR parsing
// logic nor verifying logic. It just behaves as if the passed body is a signed
// HTTP exchange which contains a request to "https://example.com/test.html" and
// a response with a payload which is equal to the original body.
// TODO(https://crbug.com/80374): Implement CBOR parsing logic and verifying
// logic.
class SignedExchangeHandler final
    : public mojo::common::DataPipeDrainer::Client {
 public:
  using ExchangeFoundCallback =
      base::OnceCallback<void(const GURL& request_url,
                              const std::string& request_method,
                              const network::ResourceResponseHead&,
                              base::Optional<net::SSLInfo>,
                              mojo::ScopedDataPipeConsumerHandle)>;
  using ExchangeFinishedCallback =
      base::OnceCallback<void(const network::URLLoaderCompletionStatus&)>;

  // The passed |body| will be read to parse the signed HTTP exchange.
  // TODO(https://crbug.com/80374): Consider making SignedExchangeHandler
  // independent from DataPipe to make it easy to port it in //net.
  explicit SignedExchangeHandler(mojo::ScopedDataPipeConsumerHandle body);

  ~SignedExchangeHandler() override;

  // TODO(https://crbug.com/80374): Currently SignedExchangeHandler always calls
  // found_callback and then calls finish_callback after reading the all buffer.
  // Need to redesign this callback model when we will introduce
  // SignedExchangeHandler::Reader class to read the body and introduce the
  // cert verification.
  void GetHTTPExchange(ExchangeFoundCallback found_callback,
                       ExchangeFinishedCallback finish_callback);

 private:
  // mojo::Common::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  // Called from |data_producer_|.
  void OnDataWritten(MojoResult result);

  mojo::ScopedDataPipeConsumerHandle body_;
  std::unique_ptr<mojo::common::DataPipeDrainer> drainer_;
  ExchangeFoundCallback found_callback_;
  ExchangeFinishedCallback finish_callback_;
  std::string original_body_string_;
  std::unique_ptr<mojo::StringDataPipeProducer> data_producer_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_
