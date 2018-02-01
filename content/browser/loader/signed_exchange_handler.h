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
#include "net/filter/filter_source_stream.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "url/gurl.h"

namespace content {

// IMPORTANT: Currenly SignedExchangeHandler doesn't implement any CBOR parsing
// logic nor verifying logic. It just behaves as if the passed body is a signed
// HTTP exchange which contains a request to "https://example.com/test.html" and
// a response with a payload which is equal to the original body.
// TODO(https://crbug.com/803774): Implement CBOR parsing logic and verifying
// logic.
class SignedExchangeHandler final : public net::FilterSourceStream {
 public:
  // TODO(https://crbug.com/803774): Add verification status here.
  using ExchangeHeadersCallback =
      base::OnceCallback<void(const GURL& request_url,
                              const std::string& request_method,
                              const network::ResourceResponseHead&,
                              base::Optional<net::SSLInfo>)>;

  // Once constructed |this| starts reading the |body| and parses the response
  // as a signed HTTP exchange. The response body of the exchange can be read
  // from |this| as a net::SourceStream after |headers_callback| is called.
  SignedExchangeHandler(std::unique_ptr<net::SourceStream> body,
                        ExchangeHeadersCallback headers_callback);
  ~SignedExchangeHandler() override;

  // net::FilterSourceStream:
  int FilterData(net::IOBuffer* output_buffer,
                 int output_buffer_size,
                 net::IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool upstream_eof_reached) override;
  std::string GetTypeAsString() const override;

 private:
  void DoHeaderLoop();
  void DidReadForHeaders(bool completed_syncly, int result);
  bool MaybeRunHeadersCallback();

  // TODO(https://crbug.com/803774): Remove this.
  void FillMockExchangeHeaders();

  // Signed exchange contents.
  GURL request_url_;
  std::string request_method_;
  network::ResourceResponseHead response_head_;
  base::Optional<net::SSLInfo> ssl_info_;

  ExchangeHeadersCallback headers_callback_;

  // Internal IOBuffer used during reading the header. Note that during parsing
  // the header we don't really need the output buffer, but we still need to
  // give some > 0 buffer.
  scoped_refptr<net::IOBufferWithSize> header_out_buf_;

  // TODO(https://crbug.cxom/803774): Just for now. Implement the streaming
  // parser.
  std::string original_body_string_;
  size_t body_string_offset_ = 0;

  base::WeakPtrFactory<SignedExchangeHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_HANDLER_H_
