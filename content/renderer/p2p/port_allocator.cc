// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/port_allocator.h"

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "content/renderer/p2p/host_address_request.h"
#include "jingle/glue/utils.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderOptions.h"

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

P2PPortAllocator::Config::Config()
    : stun_server_port(0),
      relay_server_port(0),
      legacy_relay(true),
      disable_tcp_transport(false) {
}

P2PPortAllocator::Config::~Config() {
}

P2PPortAllocator::P2PPortAllocator(
    WebKit::WebFrame* web_frame,
    P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const Config& config)
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

cricket::PortAllocatorSession* P2PPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new P2PPortAllocatorSession(
      this, content_name, component, ice_username_fragment, ice_password);
}

P2PPortAllocatorSession::P2PPortAllocatorSession(
    P2PPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password)
    : cricket::BasicPortAllocatorSession(
        allocator, content_name, component,
        ice_username_fragment, ice_password),
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
  AllocateLegacyRelaySession();
}

void P2PPortAllocatorSession::GetPortConfigurations() {
  if (!allocator_->config_.stun_server.empty() &&
      stun_server_address_.IsNil()) {
    ResolveStunServerAddress();
  } else {
    AddConfig();
  }

  if (allocator_->config_.legacy_relay) {
    AllocateLegacyRelaySession();
  }
}

void P2PPortAllocatorSession::ResolveStunServerAddress() {
  if (stun_address_request_)
    return;

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
    // Allocating local ports on stun failure.
    AddConfig();
    return;
  }

  if (!jingle_glue::IPEndPointToSocketAddress(
          net::IPEndPoint(address, allocator_->config_.stun_server_port),
          &stun_server_address_)) {
    return;
  }

  AddConfig();
}

void P2PPortAllocatorSession::AllocateLegacyRelaySession() {
  if (allocator_->config_.relay_server.empty())
    return;

  if (relay_session_attempts_ > kRelaySessionRetries)
    return;
  relay_session_attempts_++;

  relay_session_response_.clear();

  WebURLLoaderOptions options;
  options.allowCredentials = false;

  options.crossOriginRequestPolicy =
      WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl;

  relay_session_request_.reset(
      allocator_->web_frame_->createAssociatedURLLoader(options));
  if (!relay_session_request_.get()) {
    LOG(ERROR) << "Failed to create URL loader.";
    return;
  }

  std::string url = "https://" + allocator_->config_.relay_server +
      kCreateRelaySessionURL +
      "?username=" + net::EscapeUrlEncodedData(username(), true) +
      "&password=" + net::EscapeUrlEncodedData(password(), true);

  WebURLRequest request;
  request.initialize();
  request.setURL(WebURL(GURL(url)));
  request.setAllowStoredCredentials(false);
  request.setCachePolicy(WebURLRequest::ReloadIgnoringCacheData);
  request.setHTTPMethod("GET");
  request.addHTTPHeaderField(
      WebString::fromUTF8("X-Talk-Google-Relay-Auth"),
      WebString::fromUTF8(allocator_->config_.relay_password));
  request.addHTTPHeaderField(
      WebString::fromUTF8("X-Google-Relay-Auth"),
      WebString::fromUTF8(allocator_->config_.relay_password));
  request.addHTTPHeaderField(WebString::fromUTF8("X-Stream-Type"),
                             WebString::fromUTF8("chromoting"));

  relay_session_request_->loadAsynchronously(request, this);
}

void P2PPortAllocatorSession::ParseRelayResponse() {
  std::vector<std::pair<std::string, std::string> > value_pairs;
  if (!base::SplitStringIntoKeyValuePairs(relay_session_response_, '=', '\n',
                                          &value_pairs)) {
    LOG(ERROR) << "Received invalid response from relay server";
    return;
  }

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
      if (value != username()) {
        LOG(ERROR) << "When creating relay session received user name "
            " that was different from the value specified in the query.";
        return;
      }
    } else if (key == "password") {
      if (value != password()) {
        LOG(ERROR) << "When creating relay session received password "
            "that was different from the value specified in the query.";
        return;
      }
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
      new cricket::PortConfiguration(stun_server_address_, "", "");

  if (allocator_->config_.legacy_relay) {
    // Passing empty credentials for legacy google relay.
    cricket::RelayServerConfig gturn_config(cricket::RELAY_GTURN);
    if (relay_ip_.ip() != 0) {
      if (relay_udp_port_ > 0) {
        talk_base::SocketAddress address(relay_ip_.ip(), relay_udp_port_);
        gturn_config.ports.push_back(cricket::ProtocolAddress(
            address, cricket::PROTO_UDP));
      }
      if (relay_tcp_port_ > 0 && !allocator_->config_.disable_tcp_transport) {
        talk_base::SocketAddress address(relay_ip_.ip(), relay_tcp_port_);
        gturn_config.ports.push_back(cricket::ProtocolAddress(
            address, cricket::PROTO_TCP));
      }
      if (relay_ssltcp_port_ > 0 &&
          !allocator_->config_.disable_tcp_transport) {
        talk_base::SocketAddress address(relay_ip_.ip(), relay_ssltcp_port_);
        gturn_config.ports.push_back(cricket::ProtocolAddress(
            address, cricket::PROTO_SSLTCP));
      }
      if (!gturn_config.ports.empty()) {
        config->AddRelay(gturn_config);
      }
    }
  } else {
    if (!(allocator_->config_.relay_username.empty() ||
          allocator_->config_.relay_server.empty())) {
      // Adding TURN related information to config.
      // As per TURN RFC, same turn server should be used for stun as well.
      // Configuration should have same address for both stun and turn.
      DCHECK_EQ(allocator_->config_.stun_server,
                allocator_->config_.relay_server);
      cricket::RelayServerConfig turn_config(cricket::RELAY_TURN);
      cricket::RelayCredentials credentials(
          allocator_->config_.relay_username,
          allocator_->config_.relay_password);
      turn_config.credentials = credentials;
      // Using the stun resolved address if available for TURN.
      turn_config.ports.push_back(cricket::ProtocolAddress(
          stun_server_address_, cricket::PROTO_UDP));
      config->AddRelay(turn_config);
    }
  }
  ConfigReady(config);
}

}  // namespace content
