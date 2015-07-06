// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/testing_json_parser.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"

namespace safe_json {

namespace {

SafeJsonParser* CreateTestingJsonParser(
    const std::string& unsafe_json,
    const SafeJsonParser::SuccessCallback& success_callback,
    const SafeJsonParser::ErrorCallback& error_callback) {
  return new TestingJsonParser(unsafe_json, success_callback, error_callback);
}

}  // namespace

TestingJsonParser::ScopedFactoryOverride::ScopedFactoryOverride() {
  SafeJsonParser::SetFactoryForTesting(&CreateTestingJsonParser);
}

TestingJsonParser::ScopedFactoryOverride::~ScopedFactoryOverride() {
  SafeJsonParser::SetFactoryForTesting(nullptr);
}

TestingJsonParser::TestingJsonParser(const std::string& unsafe_json,
                                     const SuccessCallback& success_callback,
                                     const ErrorCallback& error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

TestingJsonParser::~TestingJsonParser() {}

void TestingJsonParser::Start() {
  int error_code;
  std::string error;
  scoped_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      unsafe_json_, base::JSON_PARSE_RFC, &error_code, &error);

  // Run the callback asynchronously.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, value ? base::Bind(success_callback_, base::Passed(&value))
                       : base::Bind(error_callback_, error));
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace
