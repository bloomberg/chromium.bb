// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/port_allocator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/p2p/host_address_request.h"
#include "jingle/glue/utils.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebURLLoaderOptions.h"

using blink::WebString;
using blink::WebURL;
using blink::WebURLLoader;
using blink::WebURLLoaderOptions;
using blink::WebURLRequest;
using blink::WebURLResponse;

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
      legacy_relay(true),
      disable_tcp_transport(false) {
}

P2PPortAllocator::Config::~Config() {
}

P2PPortAllocator::Config::RelayServerConfig::RelayServerConfig()
    : port(0) {
}

P2PPortAllocator::Config::RelayServerConfig::~RelayServerConfig() {
}

P2PPortAllocator::P2PPortAllocator(
    blink::WebFrame* web_frame,
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
  // TODO(ronghuawu): crbug/138185 add ourselves to the firewall list in browser
  // process and then remove below line.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebRtcTcpServerSocket)) {
    set_allow_tcp_listen(false);
  }
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

P2PPortAllocatorSession::RelayServer::RelayServer() {
}

P2PPortAllocatorSession::RelayServer::~RelayServer() {
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
      relay_ssltcp_port_(0),
      pending_relay_requests_(0) {
}

P2PPortAllocatorSession::~P2PPortAllocatorSession() {
  if (stun_address_request_.get())
    stun_address_request_->Cancel();

  for (size_t i = 0; i < relay_info_.size(); ++i) {
    if (relay_info_[i].relay_address_request.get())
      relay_info_[i].relay_address_request->Cancel();
  }
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

void P2PPortAllocatorSession::didFail(blink::WebURLLoader* loader,
                                      const blink::WebURLError& error) {
  DCHECK_EQ(loader, relay_session_request_.get());
  DCHECK_NE(error.reason, 0);

  LOG(ERROR) << "Relay session request failed.";

  // Retry the request.
  AllocateLegacyRelaySession();
}

void P2PPortAllocatorSession::GetPortConfigurations() {
  // Resolve Stun and Relay server addresses.
  if (!allocator_->config_.stun_server.empty() &&
      stun_server_address_.IsNil()) {
    ResolveStunServerAddress();
  } else {
    AddConfig();
  }

  if (allocator_->config_.legacy_relay) {
    AllocateLegacyRelaySession();
  } else {
    ResolveRelayServerAddresses();
  }
}

void P2PPortAllocatorSession::ResolveStunServerAddress() {
  if (stun_address_request_.get())
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

void P2PPortAllocatorSession::ResolveRelayServerAddresses() {
  for (size_t i = 0; i < allocator_->config_.relays.size(); ++i) {
    scoped_refptr<P2PHostAddressRequest> relay_request =
        new P2PHostAddressRequest(allocator_->socket_dispatcher_);
    relay_request->Request(
        allocator_->config_.relays[i].server_address,
        base::Bind(&P2PPortAllocatorSession::OnRelayServerAddressResolved,
                   base::Unretained(this), i));
    // Copy relay configuration from alloctor and keeping it in a map.
    RelayServer relay;
    relay.config = allocator_->config_.relays[i];
    relay.relay_address_request = relay_request;
    relay_info_.push_back(relay);
    ++pending_relay_requests_;
  }
}

void P2PPortAllocatorSession::OnRelayServerAddressResolved(
    size_t index, const net::IPAddressNumber& address) {
  // Let's first decrement the pending requests count.
  --pending_relay_requests_;
  if (index > relay_info_.size()) {
    NOTREACHED();
    return;
  }

  if (address.empty()) {
    LOG(ERROR) << "Failed to resolve Relay server address "
               << relay_info_.at(index).config.server_address;
  } else {
    // Getting relay server info for which this resolved address belongs.
    RelayServer& relay_server = relay_info_.at(index);

    talk_base::SocketAddress socket_address;
    if (!jingle_glue::IPEndPointToSocketAddress(
            net::IPEndPoint(address, relay_server.config.port),
            &socket_address)) {
      NOTREACHED();
    }
    relay_server.resolved_relay_address = socket_address;
  }

  if (!pending_relay_requests_)
    AddConfig();
}

void P2PPortAllocatorSession::AllocateLegacyRelaySession() {
  if (allocator_->config_.relays.empty())
    return;
  // If we are using legacy relay, we will have only one entry in relay server
  // list.
  P2PPortAllocator::Config::RelayServerConfig relay_config =
      allocator_->config_.relays[0];

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
  if (!relay_session_request_) {
    LOG(ERROR) << "Failed to create URL loader.";
    return;
  }

  std::string url = "https://" + relay_config.server_address +
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
      WebString::fromUTF8(relay_config.password));
  request.addHTTPHeaderField(
      WebString::fromUTF8("X-Google-Relay-Auth"),
      WebString::fromUTF8(relay_config.username));
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
  cricket::PortConfiguration* port_config = new cricket::PortConfiguration(
      stun_server_address_, std::string(), std::string());

  if (!pending_relay_requests_) {
    // Push all resolved addresses and transport port type to allocator.
    for (size_t i = 0; i < relay_info_.size(); ++i) {
      if (relay_info_[i].resolved_relay_address.IsNil())
        continue;

      RelayServer relay_info = relay_info_[i];
      cricket::RelayCredentials credentials(relay_info.config.username,
                                            relay_info.config.password);
      cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
      cricket::ProtocolType protocol;
      if (!cricket::StringToProto(relay_info.config.transport_type.c_str(),
                                  &protocol)) {
        DLOG(WARNING) << "Ignoring TURN server "
                      << relay_info.config.server_address << ". "
                      << "Reason= Incorrect "
                      << relay_info.config.transport_type
                      << " transport parameter.";
        continue;
      }

      relay_server.ports.push_back(cricket::ProtocolAddress(
          relay_info.resolved_relay_address,
          protocol,
          relay_info.config.secure));
      relay_server.credentials = credentials;
      port_config->AddRelay(relay_server);
    }
  }
  ConfigReady(port_config);
}

}  // namespace content
