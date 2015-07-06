// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/safe_json_parser.h"

#include "components/safe_json/safe_json_parser_impl.h"

namespace safe_json {

namespace {

SafeJsonParser::Factory g_factory = nullptr;

}  // namespace

// static
void SafeJsonParser::SetFactoryForTesting(Factory factory) {
  g_factory = factory;
}

// static
void SafeJsonParser::Parse(const std::string& unsafe_json,
                           const SuccessCallback& success_callback,
                           const ErrorCallback& error_callback) {
  SafeJsonParser* parser =
      g_factory ? g_factory(unsafe_json, success_callback, error_callback)
                : new SafeJsonParserImpl(unsafe_json, success_callback,
                                         error_callback);
  parser->Start();
}

}  // namespace safe_json
