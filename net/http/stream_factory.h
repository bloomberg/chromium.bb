// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_STREAM_FACTORY_H_

#include <string>

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"

namespace net {

class BoundNetLog;
class HostPortPair;
class HttpAlternateProtocols;
class HttpAuthController;
class HttpNetworkSession;
class HttpResponseInfo;
class HttpStream;
class ProxyInfo;
class SSLCertRequestInfo;
class SSLInfo;
class X509Certificate;
struct HttpRequestInfo;
struct SSLConfig;

// The StreamRequest is the client's handle to the worker object which handles
// the creation of an HttpStream.  While the HttpStream is being created, this
// object is the creator's handle for interacting with the HttpStream creation
// process.  The request is cancelled by deleting it, after which no callbacks
// will be invoked.
class StreamRequest {
 public:
  // The StreamRequestDelegate is a set of callback methods for a
  // StreamRequestJob.  Generally, only one of these methods will be
  // called as a result of a stream request.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // This is the success case.
    // |stream| is now owned by the delegate.
    virtual void OnStreamReady(HttpStream* stream) = 0;

    // This is the failure to create a stream case.
    virtual void OnStreamFailed(int status) = 0;

    // Called when we have a certificate error for the request.
    virtual void OnCertificateError(int status, const SSLInfo& ssl_info) = 0;

    // This is the failure case where we need proxy authentication during
    // proxy tunnel establishment.  For the tunnel case, we were unable to
    // create the HttpStream, so the caller provides the auth and then resumes
    // the StreamRequest.  For the non-tunnel case, the caller will handle
    // the authentication failure and restart the StreamRequest entirely.
    // Ownership of |auth_controller| and |proxy_response| are owned
    // by the StreamRequest.  |proxy_response| is not guaranteed to be usable
    // after the lifetime of this callback.  The delegate may take a reference
    // to |auth_controller| if it is needed beyond the lifetime of this
    // callback.
    virtual void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                                  HttpAuthController* auth_controller) = 0;

    // This is the failure for SSL Client Auth
    // Ownership of |cert_info| is retained by the StreamRequest.  The delegate
    // may take a reference if it needs the cert_info beyond the lifetime of
    // this callback.
    virtual void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) = 0;

    // This is the failure of the CONNECT request through an HTTPS proxy.
    // Headers can be read from |response_info|, while the body can be read
    // from |stream|.
    // Ownership of |stream| is transferred to the delegate.
    virtual void OnHttpsProxyTunnelResponse(
        const HttpResponseInfo& response_info, HttpStream* stream) = 0;
  };

  virtual ~StreamRequest() {}

  // When a HttpStream creation process requires a SSL Certificate,
  // the delegate OnNeedsClientAuth handler will have been called.
  // It now becomes the delegate's responsibility to collect the certificate
  // (probably from the user), and then call this method to resume
  // the HttpStream creation process.
  // Ownership of |client_cert| remains with the StreamRequest.  The
  // delegate can take a reference if needed beyond the lifetime of this
  // call.
  virtual int RestartWithCertificate(X509Certificate* client_cert) = 0;

  // When a HttpStream creation process is stalled due to necessity
  // of Proxy authentication credentials, the delegate OnNeedsProxyAuth
  // will have been called.  It now becomes the delegate's responsibility
  // to collect the necessary credentials, and then call this method to
  // resume the HttpStream creation process.
  virtual int RestartTunnelWithProxyAuth(const string16& username,
                                         const string16& password) = 0;

  // Returns the LoadState for the request.
  virtual LoadState GetLoadState() const = 0;

  // Returns true if an AlternateProtocol for this request was available.
  virtual bool was_alternate_protocol_available() const = 0;

  // Returns true if TLS/NPN was negotiated for this stream.
  virtual bool was_npn_negotiated() const = 0;

  // Returns true if this stream is being fetched over SPDY.
  virtual bool using_spdy() const = 0;
};

// The StreamFactory defines an interface for creating usable HttpStreams.
class StreamFactory {
 public:
  virtual ~StreamFactory() {}

  // Request a stream.
  // Will callback to the StreamRequestDelegate upon completion.
  // |info|, |ssl_config|, and |proxy_info| must be kept alive until
  // |delegate| is called.
  virtual StreamRequest* RequestStream(const HttpRequestInfo* info,
                                       SSLConfig* ssl_config,
                                       ProxyInfo* proxy_info,
                                       HttpNetworkSession* session,
                                       StreamRequest::Delegate* delegate,
                                       const BoundNetLog& net_log) = 0;

  // Requests that enough connections for |num_streams| be opened.  If
  // ERR_IO_PENDING is returned, |info|, |ssl_config|, and |proxy_info| must
  // be kept alive until |callback| is invoked. That callback will be given the
  // final error code.
  virtual int PreconnectStreams(int num_streams,
                                const HttpRequestInfo* info,
                                SSLConfig* ssl_config,
                                ProxyInfo* proxy_info,
                                HttpNetworkSession* session,
                                const BoundNetLog& net_log,
                                CompletionCallback* callback) = 0;

  virtual void AddTLSIntolerantServer(const GURL& url) = 0;
  virtual bool IsTLSIntolerantServer(const GURL& url) = 0;

  virtual void ProcessAlternateProtocol(
      HttpAlternateProtocols* alternate_protocols,
      const std::string& alternate_protocol_str,
      const HostPortPair& http_host_port_pair) = 0;

  virtual GURL ApplyHostMappingRules(const GURL& url,
                                     HostPortPair* endpoint) = 0;
};

}  // namespace net

#endif  // NET_HTTP_STREAM_FACTORY_H_
