// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "base/logging.h"
#include "net/base/address_family.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_filter.h"

namespace net {

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate(SSPILibrary* library,
                                                   ULONG max_token_length,
                                                   bool disable_cname_lookup,
                                                   bool use_port)
    : auth_sspi_(library, "Negotiate", NEGOSSP_NAME, max_token_length),
      user_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_cname_callback_(
          this, &HttpAuthHandlerNegotiate::OnResolveCanonicalName)),
      disable_cname_lookup_(disable_cname_lookup),
      use_port_(use_port) {
}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() {
}

int HttpAuthHandlerNegotiate::GenerateAuthToken(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  return auth_sspi_.GenerateAuthToken(
      &username,
      &password,
      spn_,
      request,
      proxy,
      auth_token);
}

// The Negotiate challenge header looks like:
//   WWW-Authenticate: NEGOTIATE auth-data
bool HttpAuthHandlerNegotiate::Init(HttpAuth::ChallengeTokenizer* challenge) {
  scheme_ = "negotiate";
  score_ = 4;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;
  return auth_sspi_.ParseChallenge(challenge);
}

// Require identity on first pass instead of second.
bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  return auth_sspi_.NeedsIdentity();
}

bool HttpAuthHandlerNegotiate::IsFinalRound() {
  return auth_sspi_.IsFinalRound();
}

bool HttpAuthHandlerNegotiate::SupportsDefaultCredentials() {
  return true;
}

bool HttpAuthHandlerNegotiate::NeedsCanonicalName() {
  if (!spn_.empty())
    return false;
  if (disable_cname_lookup_) {
    spn_ = CreateSPN(address_list_, origin_);
    address_list_.Reset();
    return false;
  }
  return true;
}

int HttpAuthHandlerNegotiate::ResolveCanonicalName(HostResolver* resolver,
                                                   CompletionCallback* callback,
                                                   const BoundNetLog& net_log) {
  // TODO(cbentzel): Add reverse DNS lookup for numeric addresses.
  DCHECK(!single_resolve_.get());
  DCHECK(!disable_cname_lookup_);
  DCHECK(callback);

  HostResolver::RequestInfo info(origin_.host(), 0);
  info.set_host_resolver_flags(HOST_RESOLVER_CANONNAME);
  single_resolve_.reset(new SingleRequestHostResolver(resolver));
  int rv = single_resolve_->Resolve(info, &address_list_,
                                    &resolve_cname_callback_,
                                    net_log);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = callback;
    return rv;
  }
  OnResolveCanonicalName(rv);
  // Always return OK. OnResolveCanonicalName logs the error code if not
  // OK and attempts to use the original origin_ hostname rather than failing
  // the auth attempt completely.
  return OK;
}

void HttpAuthHandlerNegotiate::OnResolveCanonicalName(int result) {
  if (result != OK) {
    // Even in the error case, try to use origin_.host instead of
    // passing the failure on to the caller.
    LOG(INFO) << "Problem finding canonical name for SPN for host "
              << origin_.host() << ": " << ErrorToString(result);
    result = OK;
  }
  spn_ = CreateSPN(address_list_, origin_);
  address_list_.Reset();
  if (user_callback_) {
    CompletionCallback* callback = user_callback_;
    user_callback_ = NULL;
    callback->Run(result);
  }
}

std::wstring HttpAuthHandlerNegotiate::CreateSPN(
    const AddressList& address_list, const GURL& origin) {
  // Kerberos SPNs are in the form HTTP/<host>:<port>
  //   http://msdn.microsoft.com/en-us/library/ms677601%28VS.85%29.aspx
  //
  // However, reality differs from the specification. A good description of
  // the problems can be found here:
  //   http://blog.michelbarneveld.nl/michel/archive/2009/11/14/the-reason-why-kb911149-and-kb908209-are-not-the-soluton.aspx
  //
  // Typically the <host> portion should be the canonical FQDN for the service.
  // If this could not be resolved, the original hostname in the URL will be
  // attempted instead. However, some intranets register SPNs using aliases
  // for the same canonical DNS name to allow multiple web services to reside
  // on the same host machine without requiring different ports. IE6 and IE7
  // have hotpatches that allow the default behavior to be overridden.
  //   http://support.microsoft.com/kb/911149
  //   http://support.microsoft.com/kb/938305
  //
  // According to the spec, the <port> option should be included if it is a
  // non-standard port (i.e. not 80 or 443 in the HTTP case). However,
  // historically browsers have not included the port, even on non-standard
  // ports. IE6 required a hotpatch and a registry setting to enable
  // including non-standard ports, and IE7 and IE8 also require the same
  // registry setting, but no hotpatch. Firefox does not appear to have an
  // option to include non-standard ports as of 3.6.
  //   http://support.microsoft.com/kb/908209
  //
  // Without any command-line flags, Chrome matches the behavior of Firefox
  // and IE. Users can override the behavior so aliases are allowed and
  // non-standard ports are included.
  int port = origin.EffectiveIntPort();
  std::string server;
  if (!address_list.GetCanonicalName(&server))
    server = origin.host();
  if (port != 80 && port != 443 && use_port_) {
    return ASCIIToWide(StringPrintf("HTTP/%s:%d", server.c_str(), port));
  } else {
    return ASCIIToWide(StringPrintf("HTTP/%s", server.c_str()));
  }
}

int HttpAuthHandlerNegotiate::GenerateDefaultAuthToken(
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  return auth_sspi_.GenerateAuthToken(
      NULL,  // username
      NULL,  // password
      spn_,
      request,
      proxy,
      auth_token);
}

HttpAuthHandlerNegotiate::Factory::Factory()
    : disable_cname_lookup_(false),
      use_port_(false),
      max_token_length_(0),
      first_creation_(true),
      is_unsupported_(false),
      sspi_library_(SSPILibrary::GetDefault()) {
}

HttpAuthHandlerNegotiate::Factory::~Factory() {
}

int HttpAuthHandlerNegotiate::Factory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    scoped_refptr<HttpAuthHandler>* handler) {
  if (is_unsupported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  if (max_token_length_ == 0) {
    int rv = DetermineMaxTokenLength(sspi_library_, NEGOSSP_NAME,
                                     &max_token_length_);
    if (rv == ERR_UNSUPPORTED_AUTH_SCHEME)
      is_unsupported_ = true;
    if (rv != OK)
      return rv;
  }
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  scoped_refptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerNegotiate(sspi_library_, max_token_length_,
                                   disable_cname_lookup_, use_port_));
  if (!tmp_handler->InitFromChallenge(challenge, target, origin))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

}  // namespace net
