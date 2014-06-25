// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <string>

#include "content/public/common/content_constants.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"

namespace IPC {

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  DCHECK(p.possibly_invalid_spec().length() <= content::GetMaxURLChars());

  // Beware of print-parse inconsistency which would change an invalid
  // URL into a valid one. Ideally, the message would contain this flag
  // so that the read side could make the check, but performing it here
  // avoids changing the on-the-wire representation of such a fundamental
  // type as GURL. See https://crbug.com/166486 for additional work in
  // this area.
  if (!p.is_valid()) {
    m->WriteString(std::string());
    return;
  }

  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const Message* m, PickleIterator* iter, GURL* p) {
  std::string s;
  if (!m->ReadString(iter, &s) || s.length() > content::GetMaxURLChars()) {
    *p = GURL();
    return false;
  }
  *p = GURL(s);
  if (!s.empty() && !p->is_valid()) {
    *p = GURL();
    return false;
  }
  return true;
}

void ParamTraits<GURL>::Log(const GURL& p, std::string* l) {
  l->append(p.spec());
}

void ParamTraits<url::Origin>::Write(Message* m,
                                          const url::Origin& p) {
  m->WriteString(p.string());
}

bool ParamTraits<url::Origin>::Read(const Message* m,
                                    PickleIterator* iter,
                                    url::Origin* p) {
  std::string s;
  if (!m->ReadString(iter, &s)) {
    *p = url::Origin();
    return false;
  }
  *p = url::Origin(s);
  return true;
}

void ParamTraits<url::Origin>::Log(const url::Origin& p, std::string* l) {
  l->append(p.string());
}

void ParamTraits<net::HostPortPair>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const Message* m,
                                          PickleIterator* iter,
                                          param_type* r) {
  std::string host;
  uint16 port;
  if (!ReadParam(m, iter, &host) || !ReadParam(m, iter, &port))
    return false;

  r->set_host(host);
  r->set_port(port);
  return true;
}

void ParamTraits<net::HostPortPair>::Log(const param_type& p, std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<net::IPEndPoint>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.address());
  WriteParam(m, p.port());
}

bool ParamTraits<net::IPEndPoint>::Read(const Message* m, PickleIterator* iter,
                                        param_type* p) {
  net::IPAddressNumber address;
  int port;
  if (!ReadParam(m, iter, &address) || !ReadParam(m, iter, &port))
    return false;
  if (address.size() &&
      address.size() != net::kIPv4AddressSize &&
      address.size() != net::kIPv6AddressSize) {
    return false;
  }
  *p = net::IPEndPoint(address, port);
  return true;
}

void ParamTraits<net::IPEndPoint>::Log(const param_type& p, std::string* l) {
  LogParam("IPEndPoint:" + p.ToString(), l);
}

void ParamTraits<content::PageState>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.ToEncodedData());
}

bool ParamTraits<content::PageState>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  std::string data;
  if (!ReadParam(m, iter, &data))
    return false;
  *r = content::PageState::CreateFromEncodedData(data);
  return true;
}

void ParamTraits<content::PageState>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.ToEncodedData(), l);
  l->append(")");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC
