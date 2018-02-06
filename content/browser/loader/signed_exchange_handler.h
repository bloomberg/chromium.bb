// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/completion_callback.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "url/gurl.h"

namespace net {
class SourceStream;
}

namespace content {

// IMPORTANT: Currenly SignedExchangeHandler doesn't implement any verifying
// logic.
// TODO(https://crbug.com/803774): Implement verifying logic.
class SignedExchangeHandler final {
 public:
  // TODO(https://crbug.com/803774): Add verification status here.
  using ExchangeHeadersCallback =
      base::OnceCallback<void(net::Error error,
                              const GURL& request_url,
                              const std::string& request_method,
                              const network::ResourceResponseHead&,
                              std::unique_ptr<net::SourceStream> payload_stream,
                              base::Optional<net::SSLInfo>)>;

  // Once constructed |this| starts reading the |body| and parses the response
  // as a signed HTTP exchange. The response body of the exchange can be read
  // from |payload_stream| passed to |headers_callback|.
  SignedExchangeHandler(std::unique_ptr<net::SourceStream> body,
                        ExchangeHeadersCallback headers_callback);
  ~SignedExchangeHandler();

 private:
  void ReadLoop();
  void DidRead(bool completed_syncly, int result);
  bool RunHeadersCallback();
  void RunErrorCallback(net::Error);

  // Signed exchange contents.
  GURL request_url_;
  std::string request_method_;
  network::ResourceResponseHead response_head_;
  base::Optional<net::SSLInfo> ssl_info_;

  ExchangeHeadersCallback headers_callback_;
  std::unique_ptr<net::SourceStream> source_;

  // TODO(https://crbug.cxom/803774): Just for now. Implement the streaming
  // parser.
  scoped_refptr<net::IOBufferWithSize> read_buf_;
  std::string original_body_string_;

  base::WeakPtrFactory<SignedExchangeHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_
