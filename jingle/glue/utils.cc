// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/utils.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/p2p/base/candidate.h"

namespace jingle_glue {

bool IPEndPointToSocketAddress(const net::IPEndPoint& address_chrome,
                               talk_base::SocketAddress* address_lj) {
  if (address_chrome.GetFamily() != net::ADDRESS_FAMILY_IPV4) {
    LOG(ERROR) << "Only IPv4 addresses are supported.";
    return false;
  }
  uint32 ip_as_int = talk_base::NetworkToHost32(
      *reinterpret_cast<const uint32*>(&address_chrome.address()[0]));
  *address_lj =  talk_base::SocketAddress(ip_as_int, address_chrome.port());
  return true;
}

bool SocketAddressToIPEndPoint(const talk_base::SocketAddress& address_lj,
                               net::IPEndPoint* address_chrome) {
  uint32 ip = talk_base::HostToNetwork32(address_lj.ip());
  net::IPAddressNumber address;
  address.resize(net::kIPv4AddressSize);
  memcpy(&address[0], &ip, net::kIPv4AddressSize);
  *address_chrome = net::IPEndPoint(address, address_lj.port());
  return true;
}

std::string SerializeP2PCandidate(const cricket::Candidate& candidate) {
  // TODO(sergeyu): Use SDP to format candidates?
  DictionaryValue value;
  value.SetString("ip", candidate.address().ipaddr().ToString());
  value.SetInteger("port", candidate.address().port());
  value.SetString("type", candidate.type());
  value.SetString("protocol", candidate.protocol());
  value.SetString("username", candidate.username());
  value.SetString("password", candidate.password());
  value.SetDouble("preference", candidate.preference());
  value.SetInteger("generation", candidate.generation());

  std::string result;
  base::JSONWriter::Write(&value, &result);
  return result;
}

bool DeserializeP2PCandidate(const std::string& candidate_str,
                             cricket::Candidate* candidate) {
  scoped_ptr<Value> value(
      base::JSONReader::Read(candidate_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    return false;
  }

  DictionaryValue* dic_value = static_cast<DictionaryValue*>(value.get());

  std::string ip;
  int port;
  std::string type;
  std::string protocol;
  std::string username;
  std::string password;
  double preference;
  int generation;

  if (!dic_value->GetString("ip", &ip) ||
      !dic_value->GetInteger("port", &port) ||
      !dic_value->GetString("type", &type) ||
      !dic_value->GetString("protocol", &protocol) ||
      !dic_value->GetString("username", &username) ||
      !dic_value->GetString("password", &password) ||
      !dic_value->GetDouble("preference", &preference) ||
      !dic_value->GetInteger("generation", &generation)) {
    return false;
  }

  candidate->set_address(talk_base::SocketAddress(ip, port));
  candidate->set_type(type);
  candidate->set_protocol(protocol);
  candidate->set_username(username);
  candidate->set_password(password);
  candidate->set_preference(static_cast<float>(preference));
  candidate->set_generation(generation);

  return true;
}

}  // namespace jingle_glue
