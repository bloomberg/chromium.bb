// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/completion_callback.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class SourceStream;
}

namespace content {

class SharedURLLoaderFactory;
class SignedExchangeCertFetcher;
class URLLoaderThrottle;
class MerkleIntegritySourceStream;

// IMPORTANT: Currenly SignedExchangeHandler doesn't implement any verifying
// logic.
// TODO(https://crbug.com/803774): Implement verifying logic.
class CONTENT_EXPORT SignedExchangeHandler {
 public:
  // TODO(https://crbug.com/803774): Add verification status here.
  using ExchangeHeadersCallback =
      base::OnceCallback<void(net::Error error,
                              const GURL& request_url,
                              const std::string& request_method,
                              const network::ResourceResponseHead&,
                              std::unique_ptr<net::SourceStream> payload_stream,
                              base::Optional<net::SSLInfo>)>;

  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;

  // Once constructed |this| starts reading the |body| and parses the response
  // as a signed HTTP exchange. The response body of the exchange can be read
  // from |payload_stream| passed to |headers_callback|. |url_loader_factory|
  // and |url_loader_throttles_getter| are used to set up a network URLLoader to
  // actually fetch the certificate.
  SignedExchangeHandler(
      std::unique_ptr<net::SourceStream> body,
      ExchangeHeadersCallback headers_callback,
      url::Origin request_initiator,
      scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter);
  ~SignedExchangeHandler();

 protected:
  SignedExchangeHandler();

 private:
  void ReadLoop();
  void DidRead(bool completed_syncly, int result);
  bool RunHeadersCallback();
  void RunErrorCallback(net::Error);

  void OnCertReceived(scoped_refptr<net::X509Certificate> cert);

  // Signed exchange contents.
  GURL request_url_;
  std::string request_method_;
  network::ResourceResponseHead response_head_;

  ExchangeHeadersCallback headers_callback_;
  std::unique_ptr<net::SourceStream> source_;

  // TODO(https://crbug.cxom/803774): Just for now. Implement the streaming
  // parser.
  scoped_refptr<net::IOBufferWithSize> read_buf_;
  std::string original_body_string_;

  std::unique_ptr<MerkleIntegritySourceStream> mi_stream_;

  // Used to create |cert_fetcher_|.
  url::Origin request_initiator_;
  scoped_refptr<SharedURLLoaderFactory> url_loader_factory_;
  // This getter is guaranteed to be valid at least until the headers callback
  // is run.
  URLLoaderThrottlesGetter url_loader_throttles_getter_;

  std::unique_ptr<SignedExchangeCertFetcher> cert_fetcher_;

  base::WeakPtrFactory<SignedExchangeHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeHandler);
};

// Used only for testing.
class SignedExchangeHandlerFactory {
 public:
  virtual ~SignedExchangeHandlerFactory() {}

  virtual std::unique_ptr<SignedExchangeHandler> Create(
      std::unique_ptr<net::SourceStream> body,
      SignedExchangeHandler::ExchangeHeadersCallback headers_callback,
      url::Origin request_initiator,
      scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
      SignedExchangeHandler::URLLoaderThrottlesGetter
          url_loader_throttles_getter) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_
