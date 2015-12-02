// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_browser.h"

using Options = headless::HeadlessBrowser::Options;
using Builder = headless::HeadlessBrowser::Options::Builder;

namespace headless {

Options::Options(int argc, const char** argv)
    : argc(argc), argv(argv), devtools_http_port(kInvalidPort) {}

Options::~Options() {}

Builder::Builder(int argc, const char** argv) : options_(argc, argv) {}

Builder::~Builder() {}

Builder& Builder::SetUserAgent(const std::string& user_agent) {
  options_.user_agent = user_agent;
  return *this;
}

Builder& Builder::EnableDevToolsServer(int port) {
  options_.devtools_http_port = port;
  return *this;
}

Builder& Builder::SetURLRequestContextGetter(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  options_.url_request_context_getter = url_request_context_getter;
  return *this;
}

Options Builder::Build() {
  return options_;
}

}  // namespace headless
