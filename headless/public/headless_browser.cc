// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/user_agent.h"
#include "headless/public/headless_browser.h"
#include "net/url_request/url_request_context_getter.h"

using Options = headless::HeadlessBrowser::Options;
using Builder = headless::HeadlessBrowser::Options::Builder;

namespace headless {

// Product name for building the default user agent string.
namespace {
const char kProductName[] = "HeadlessChrome";
}

Options::Options(int argc, const char** argv)
    : argc(argc),
      argv(argv),
      user_agent(content::BuildUserAgentFromProduct(kProductName)),
      message_pump(nullptr) {}

Options::Options(const Options& other) = default;

Options::~Options() {}

Builder::Builder(int argc, const char** argv) : options_(argc, argv) {}

Builder::Builder() : options_(0, nullptr) {}

Builder::~Builder() {}

Builder& Builder::SetUserAgent(const std::string& user_agent) {
  options_.user_agent = user_agent;
  return *this;
}

Builder& Builder::EnableDevToolsServer(const net::IPEndPoint& endpoint) {
  options_.devtools_endpoint = endpoint;
  return *this;
}

Builder& Builder::SetMessagePump(base::MessagePump* message_pump) {
  options_.message_pump = message_pump;
  return *this;
}

Builder& Builder::SetProxyServer(const net::HostPortPair& proxy_server) {
  options_.proxy_server = proxy_server;
  return *this;
}

Builder& Builder::SetHostResolverRules(const std::string& host_resolver_rules) {
  options_.host_resolver_rules = host_resolver_rules;
  return *this;
}

Options Builder::Build() {
  return options_;
}

}  // namespace headless
