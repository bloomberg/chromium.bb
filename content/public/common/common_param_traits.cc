// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <string>

#include "content/public/common/content_constants.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace IPC {

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  if (p.possibly_invalid_spec().length() > content::kMaxURLChars) {
    m->WriteString(std::string());
    return;
  }

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

bool ParamTraits<GURL>::Read(const Message* m,
                             base::PickleIterator* iter,
                             GURL* p) {
  std::string s;
  if (!iter->ReadString(&s) || s.length() > content::kMaxURLChars) {
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

void ParamTraits<url::Origin>::Write(Message* m, const url::Origin& p) {
  WriteParam(m, p.unique());
  WriteParam(m, p.scheme());
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<url::Origin>::Read(const Message* m,
                                    base::PickleIterator* iter,
                                    url::Origin* p) {
  bool unique;
  std::string scheme;
  std::string host;
  uint16_t port;
  if (!ReadParam(m, iter, &unique) || !ReadParam(m, iter, &scheme) ||
      !ReadParam(m, iter, &host) || !ReadParam(m, iter, &port)) {
    *p = url::Origin();
    return false;
  }

  *p = unique ? url::Origin()
              : url::Origin::UnsafelyCreateOriginWithoutNormalization(
                    scheme, host, port);

  // If a unique origin was created, but the unique flag wasn't set, then
  // the values provided to 'UnsafelyCreateOriginWithoutNormalization' were
  // invalid; kill the renderer.
  if (!unique && p->unique())
    return false;

  return true;
}

void ParamTraits<url::Origin>::Log(const url::Origin& p, std::string* l) {
  l->append(p.Serialize());
}

void ParamTraits<net::HostPortPair>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const Message* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  std::string host;
  uint16_t port;
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

bool ParamTraits<net::IPEndPoint>::Read(const Message* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  net::IPAddressNumber address;
  uint16_t port;
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

void ParamTraits<net::IPAddress>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.bytes());
}

bool ParamTraits<net::IPAddress>::Read(const Message* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  std::vector<uint8_t> bytes;
  if (!ReadParam(m, iter, &bytes))
    return false;
  if (bytes.size() &&
      bytes.size() != net::IPAddress::kIPv4AddressSize &&
      bytes.size() != net::IPAddress::kIPv6AddressSize) {
    return false;
  }
  *p = net::IPAddress(bytes);
  return true;
}

void ParamTraits<net::IPAddress>::Log(const param_type& p, std::string* l) {
    LogParam("IPAddress:" + (p.empty() ? "(empty)" : p.ToString()), l);
}

void ParamTraits<content::PageState>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.ToEncodedData());
}

bool ParamTraits<content::PageState>::Read(const Message* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
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
