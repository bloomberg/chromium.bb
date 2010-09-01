// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_STREAM_FACTORY_H_

#include <string>

#include "base/ref_counted.h"
#include "net/base/load_states.h"

namespace net {

class BoundNetLog;
class HostPortPair;
class HttpAlternateProtocols;
class HttpAuthController;
class HttpNetworkSession;
class HttpResponseInfo;
class HttpStream;
class HttpStreamHandle;
class ProxyInfo;
class SSLCertRequestInfo;
class SSLInfo;
class X509Certificate;
struct HttpRequestInfo;
struct SSLConfig;

// The StreamFactory defines an interface for creating usable HttpStreams.
class StreamFactory {
 public:
  // The StreamRequestDelegate is a set of callback methods for a
  // StreamRequestJob.  Generally, only one of these methods will be
  // called as a result of a stream request.
  class StreamRequestDelegate {
   public:
    virtual ~StreamRequestDelegate() {}

    // This is the success case.
    // |stream| is now owned by the delegate.
    virtual void OnStreamReady(HttpStreamHandle* stream) = 0;

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
  };

  // The StreamRequestJob is the worker object which handles the creation
  // of an HttpStream.  While the HttpStream is being created, this job
  // is the creator's handle for interacting with the HttpStream creation
  // process.
  class StreamRequestJob : public base::RefCounted<StreamRequestJob> {
   public:
    virtual ~StreamRequestJob() {}

    // Start initiates the process of creating a new HttpStream.
    // 3 parameters are passed in by reference.  The caller asserts that the
    // lifecycle of these parameters will  remain valid until the stream is
    // created, failed, or until the caller calls Cancel() on the stream
    // request.  In all cases, the delegate will be called to notify
    // completion of the request.
    virtual void Start(const HttpRequestInfo* request_info,
                       SSLConfig* ssl_config,
                       ProxyInfo* proxy_info,
                       StreamRequestDelegate* delegate,
                       const BoundNetLog& net_log) = 0;

    // Cancel can be used to abort the HttpStream creation.  Once cancelled,
    // the delegates associated with the request will not be invoked.
    virtual void Cancel() = 0;

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

  virtual ~StreamFactory() {}

  // Request a stream.
  // Will callback to the StreamRequestDelegate upon completion.
  virtual void RequestStream(const HttpRequestInfo* info,
                             SSLConfig* ssl_config,
                             ProxyInfo* proxy_info,
                             StreamRequestDelegate* delegate,
                             const BoundNetLog& net_log,
                             const scoped_refptr<HttpNetworkSession>& session,
                             scoped_refptr<StreamRequestJob>* stream) = 0;

  // TLS Intolerant Server API
  virtual void AddTLSIntolerantServer(const GURL& url) = 0;
  virtual bool IsTLSIntolerantServer(const GURL& url) = 0;

  // Alternate Protocol API
  virtual void ProcessAlternateProtocol(
      HttpAlternateProtocols* alternate_protocols,
      const std::string& alternate_protocol_str,
      const HostPortPair& http_host_port_pair) = 0;
};

}  // namespace net

#endif  // NET_HTTP_STREAM_FACTORY_H_

