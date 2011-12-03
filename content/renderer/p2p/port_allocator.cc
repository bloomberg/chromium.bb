// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/port_allocator.h"

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "content/renderer/p2p/host_address_request.h"
#include "jingle/glue/utils.h"
#include "net/base/ip_endpoint.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"

using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderOptions;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace content {

namespace {

// URL used to create a relay session.
const char kCreateRelaySessionURL[] = "/create_session";

// Number of times we will try to request relay session.
const int kRelaySessionRetries = 3;

// Manimum relay server size we would try to parse.
const int kMaximumRelayResponseSize = 102400;

bool ParsePortNumber(
    const std::string& string, int* value) {
  if (!base::StringToInt(string, value) || *value <= 0 || *value >= 65536) {
    LOG(ERROR) << "Received invalid port number from relay server: " << string;
    return false;
  }
  return true;
}

}  // namespace

P2PPortAllocator::P2PPortAllocator(
    WebKit::WebFrame* web_frame,
    P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const webkit_glue::P2PTransport::Config& config)
    : cricket::BasicPortAllocator(network_manager, socket_factory),
      web_frame_(web_frame),
      socket_dispatcher_(socket_dispatcher),
      config_(config) {
  uint32 flags = 0;
  if (config_.disable_tcp_transport)
    flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
  set_flags(flags);
}

P2PPortAllocator::~P2PPortAllocator() {
}

cricket::PortAllocatorSession* P2PPortAllocator::CreateSession(
    const std::string& name,
    const std::string& session_type) {
  return new P2PPortAllocatorSession(this, name, session_type);
}

P2PPortAllocatorSession::P2PPortAllocatorSession(
    P2PPortAllocator* allocator,
    const std::string& name,
    const std::string& session_type)
    : cricket::BasicPortAllocatorSession(allocator, name, session_type),
      allocator_(allocator),
      relay_session_attempts_(0),
      relay_udp_port_(0),
      relay_tcp_port_(0),
      relay_ssltcp_port_(0) {
}

P2PPortAllocatorSession::~P2PPortAllocatorSession() {
  if (stun_address_request_)
    stun_address_request_->Cancel();
}

void P2PPortAllocatorSession::didReceiveData(
    WebURLLoader* loader, const char* data,
    int data_length, int encoded_data_length) {
  DCHECK_EQ(loader, relay_session_request_.get());
  if (static_cast<int>(relay_session_response_.size()) + data_length >
      kMaximumRelayResponseSize) {
    LOG(ERROR) << "Response received from the server is too big.";
    loader->cancel();
    return;
  }
  relay_session_response_.append(data, data + data_length);
}

void P2PPortAllocatorSession::didFinishLoading(WebURLLoader* loader,
                                               double finish_time) {
  ParseRelayResponse();
}

void P2PPortAllocatorSession::didFail(WebKit::WebURLLoader* loader,
                                      const WebKit::WebURLError& error) {
  DCHECK_EQ(loader, relay_session_request_.get());
  DCHECK_NE(error.reason, 0);

  LOG(ERROR) << "Relay session request failed.";

  // Retry the request.
  AllocateRelaySession();
}

void P2PPortAllocatorSession::GetPortConfigurations() {
  // Add an empty configuration synchronously, so a local connection
  // can be started immediately.
  ConfigReady(new cricket::PortConfiguration(
      talk_base::SocketAddress(), "", "", ""));

  ResolveStunServerAddress();
  AllocateRelaySession();
}

void P2PPortAllocatorSession::ResolveStunServerAddress() {
 if (allocator_->config_.stun_server.empty())
   return;

 DCHECK(!stun_address_request_);
 stun_address_request_ =
     new P2PHostAddressRequest(allocator_->socket_dispatcher_);
 stun_address_request_->Request(allocator_->config_.stun_server, base::Bind(
     &P2PPortAllocatorSession::OnStunServerAddress,
     base::Unretained(this)));
}

void P2PPortAllocatorSession::OnStunServerAddress(
    const net::IPAddressNumber& address) {
  if (address.empty()) {
    LOG(ERROR) << "Failed to resolve STUN server address "
               << allocator_->config_.stun_server;
    return;
  }

  if (!jingle_glue::IPEndPointToSocketAddress(
          net::IPEndPoint(address, allocator_->config_.stun_server_port),
          &stun_server_address_)) {
    return;
  }

  AddConfig();
}

void P2PPortAllocatorSession::AllocateRelaySession() {
  if (allocator_->config_.relay_server.empty())
    return;

  if (!allocator_->config_.legacy_relay) {
    NOTIMPLEMENTED() << " TURN support is not implemented yet.";
    return;
  }

  if (relay_session_attempts_ > kRelaySessionRetries)
    return;
  relay_session_attempts_++;

  relay_session_response_.clear();

  WebURLLoaderOptions options;
  options.allowCredentials = false;

  // TODO(sergeyu): Set to CrossOriginRequestPolicyUseAccessControl
  // when this code can be used by untrusted plugins.
  // See http://crbug.com/104195 .
  options.crossOriginRequestPolicy =
      WebURLLoaderOptions::CrossOriginRequestPolicyAllow;

  relay_session_request_.reset(
      allocator_->web_frame_->createAssociatedURLLoader(options));
  if (!relay_session_request_.get()) {
    LOG(ERROR) << "Failed to create URL loader.";
    return;
  }

  WebURLRequest request;
  request.initialize();
  request.setURL(WebURL(GURL(
      "https://" + allocator_->config_.relay_server + kCreateRelaySessionURL)));
  request.setAllowStoredCredentials(false);
  request.setCachePolicy(WebURLRequest::ReloadIgnoringCacheData);
  request.setHTTPMethod("GET");
  request.addHTTPHeaderField(
      WebString::fromUTF8("X-Talk-Google-Relay-Auth"),
      WebString::fromUTF8(allocator_->config_.relay_password));
  request.addHTTPHeaderField(
      WebString::fromUTF8("X-Google-Relay-Auth"),
      WebString::fromUTF8(allocator_->config_.relay_password));
  request.addHTTPHeaderField(WebString::fromUTF8("X-Session-Type"),
                             WebString::fromUTF8(session_type()));
  request.addHTTPHeaderField(WebString::fromUTF8("X-Stream-Type"),
                             WebString::fromUTF8(name()));

  relay_session_request_->loadAsynchronously(request, this);
}

void P2PPortAllocatorSession::ParseRelayResponse() {
  std::vector<std::pair<std::string, std::string> > value_pairs;
  if (!base::SplitStringIntoKeyValuePairs(relay_session_response_, '=', '\n',
                                          &value_pairs)) {
    LOG(ERROR) << "Received invalid response from relay server";
    return;
  }

  relay_username_.clear();
  relay_password_.clear();
  relay_ip_.Clear();
  relay_udp_port_ = 0;
  relay_tcp_port_ = 0;
  relay_ssltcp_port_ = 0;

  for (std::vector<std::pair<std::string, std::string> >::iterator
           it = value_pairs.begin();
       it != value_pairs.end(); ++it) {
    std::string key;
    std::string value;
    TrimWhitespaceASCII(it->first, TRIM_ALL, &key);
    TrimWhitespaceASCII(it->second, TRIM_ALL, &value);

    if (key == "username") {
      relay_username_ = value;
    } else if (key == "password") {
      relay_password_ = value;
    } else if (key == "relay.ip") {
      relay_ip_.SetIP(value);
      if (relay_ip_.ip() == 0) {
        LOG(ERROR) << "Received unresolved relay server address: " << value;
        return;
      }
    } else if (key == "relay.udp_port") {
      if (!ParsePortNumber(value, &relay_udp_port_))
        return;
    } else if (key == "relay.tcp_port") {
      if (!ParsePortNumber(value, &relay_tcp_port_))
        return;
    } else if (key == "relay.ssltcp_port") {
      if (!ParsePortNumber(value, &relay_ssltcp_port_))
        return;
    }
  }

  AddConfig();
}

void P2PPortAllocatorSession::AddConfig() {
  cricket::PortConfiguration* config =
      new cricket::PortConfiguration(stun_server_address_,
                                     relay_username_, relay_password_, "");

  cricket::PortConfiguration::PortList ports;
  if (relay_ip_.ip() != 0) {
    if (relay_udp_port_ > 0) {
      talk_base::SocketAddress address(relay_ip_.ip(), relay_udp_port_);
      ports.push_back(cricket::ProtocolAddress(address, cricket::PROTO_UDP));
    }
    if (relay_tcp_port_ > 0 && !allocator_->config_.disable_tcp_transport) {
      talk_base::SocketAddress address(relay_ip_.ip(), relay_tcp_port_);
      ports.push_back(cricket::ProtocolAddress(address, cricket::PROTO_TCP));
    }
    if (relay_ssltcp_port_ > 0 && !allocator_->config_.disable_tcp_transport) {
      talk_base::SocketAddress address(relay_ip_.ip(), relay_ssltcp_port_);
      ports.push_back(cricket::ProtocolAddress(address, cricket::PROTO_SSLTCP));
    }
  }
  config->AddRelay(ports, 0.0f);
  ConfigReady(config);
}

}  // namespace content
