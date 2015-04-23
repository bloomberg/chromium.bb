// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOCK_PROXY_RESOLVER_H_
#define NET_PROXY_MOCK_PROXY_RESOLVER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "url/gurl.h"

namespace base {
class MessageLoop;
}

namespace net {

// Asynchronous mock proxy resolver. All requests complete asynchronously,
// user must call Request::CompleteNow() on a pending request to signal it.
class MockAsyncProxyResolverBase : public ProxyResolver {
 public:
  class Request : public base::RefCounted<Request> {
   public:
    Request(MockAsyncProxyResolverBase* resolver,
            const GURL& url,
            ProxyInfo* results,
            const CompletionCallback& callback);

    const GURL& url() const { return url_; }
    ProxyInfo* results() const { return results_; }
    const CompletionCallback& callback() const { return callback_; }

    void CompleteNow(int rv);

   private:
    friend class base::RefCounted<Request>;

    virtual ~Request();

    MockAsyncProxyResolverBase* resolver_;
    const GURL url_;
    ProxyInfo* results_;
    CompletionCallback callback_;
    base::MessageLoop* origin_loop_;
  };

  class SetPacScriptRequest {
   public:
    SetPacScriptRequest(
        MockAsyncProxyResolverBase* resolver,
        const scoped_refptr<ProxyResolverScriptData>& script_data,
        const CompletionCallback& callback);
    ~SetPacScriptRequest();

    const ProxyResolverScriptData* script_data() const {
      return script_data_.get();
    }

    void CompleteNow(int rv);

   private:
    MockAsyncProxyResolverBase* resolver_;
    const scoped_refptr<ProxyResolverScriptData> script_data_;
    CompletionCallback callback_;
    base::MessageLoop* origin_loop_;
  };

  typedef std::vector<scoped_refptr<Request> > RequestsList;

  ~MockAsyncProxyResolverBase() override;

  // ProxyResolver implementation.
  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const CompletionCallback& callback,
                     RequestHandle* request_handle,
                     const BoundNetLog& /*net_log*/) override;
  void CancelRequest(RequestHandle request_handle) override;
  LoadState GetLoadState(RequestHandle request_handle) const override;
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& script_data,
                   const CompletionCallback& callback) override;
  void CancelSetPacScript() override;

  const RequestsList& pending_requests() const {
    return pending_requests_;
  }

  const RequestsList& cancelled_requests() const {
    return cancelled_requests_;
  }

  SetPacScriptRequest* pending_set_pac_script_request() const;

  bool has_pending_set_pac_script_request() const {
    return pending_set_pac_script_request_.get() != NULL;
  }

  void RemovePendingRequest(Request* request);

  void RemovePendingSetPacScriptRequest(SetPacScriptRequest* request);

 protected:
  explicit MockAsyncProxyResolverBase(bool expects_pac_bytes);

 private:
  RequestsList pending_requests_;
  RequestsList cancelled_requests_;
  scoped_ptr<SetPacScriptRequest> pending_set_pac_script_request_;
};

class MockAsyncProxyResolver : public MockAsyncProxyResolverBase {
 public:
  MockAsyncProxyResolver()
      : MockAsyncProxyResolverBase(false /*expects_pac_bytes*/) {}
};

class MockAsyncProxyResolverExpectsBytes : public MockAsyncProxyResolverBase {
 public:
  MockAsyncProxyResolverExpectsBytes()
      : MockAsyncProxyResolverBase(true /*expects_pac_bytes*/) {}
};

// This factory returns ProxyResolvers that forward all requests to
// |resolver|. |resolver| must remain so long as any value returned from
// CreateProxyResolver remains in use.
class ForwardingProxyResolverFactory : public LegacyProxyResolverFactory {
 public:
  explicit ForwardingProxyResolverFactory(ProxyResolver* resolver);

  // LegacyProxyResolverFactory override.
  scoped_ptr<ProxyResolver> CreateProxyResolver() override;

 private:
  ProxyResolver* resolver_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingProxyResolverFactory);
};

// ForwardingProxyResolver forwards all requests to |impl|. |impl| must remain
// so long as this remains in use.
class ForwardingProxyResolver : public ProxyResolver {
 public:
  explicit ForwardingProxyResolver(ProxyResolver* impl);

  // ProxyResolver overrides.
  int GetProxyForURL(const GURL& query_url,
                     ProxyInfo* results,
                     const CompletionCallback& callback,
                     RequestHandle* request,
                     const BoundNetLog& net_log) override;
  void CancelRequest(RequestHandle request) override;
  LoadState GetLoadState(RequestHandle request) const override;
  void CancelSetPacScript() override;
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& script_data,
                   const CompletionCallback& callback) override;

 private:
  ProxyResolver* impl_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingProxyResolver);
};

}  // namespace net

#endif  // NET_PROXY_MOCK_PROXY_RESOLVER_H_
