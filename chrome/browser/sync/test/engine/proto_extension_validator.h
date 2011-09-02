// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A test utility that makes it easy to assert the exact presence of
// protobuf extensions in an extensible message.  Example code:
//
// sync_pb::EntitySpecifics value;   // Set up a test value.
// value.MutableExtension(sync_pb::bookmark);
//
// ProtoExtensionValidator<sync_pb::EntitySpecifics> good_expectation(value);
// good_expectation.ExpectHasExtension(sync_pb::bookmark);
// good_expectation.ExpectHasNoOtherFieldsOrExtensions();
//
// ProtoExtensionValidator<sync_pb::EntitySpecifics> bad_expectation(value);
// bad_expectation.ExpectHasExtension(sync_pb::preference);  // Fails, no pref
// bad_expectation.ExpectHasNoOtherFieldsOrExtensions();  // Fails, bookmark

#ifndef CHROME_BROWSER_SYNC_TEST_ENGINE_PROTO_EXTENSION_VALIDATOR_H_
#define CHROME_BROWSER_SYNC_TEST_ENGINE_PROTO_EXTENSION_VALIDATOR_H_
#pragma once

#include "base/string_util.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

template<typename MessageType>
class ProtoExtensionValidator {
 public:
  explicit ProtoExtensionValidator(const MessageType& proto)
    : proto_(proto) {
  }
  template<typename ExtensionTokenType>
  void ExpectHasExtension(ExtensionTokenType token) {
    EXPECT_TRUE(proto_.HasExtension(token))
        << "Proto failed to contain expected extension";
    proto_.ClearExtension(token);
    EXPECT_FALSE(proto_.HasExtension(token));
  }
  void ExpectNoOtherFieldsOrExtensions() {
    EXPECT_EQ(MessageType::default_instance().SerializeAsString(),
              proto_.SerializeAsString())
        << "Proto contained additional unexpected extensions.";
  }

 private:
  MessageType proto_;
  DISALLOW_COPY_AND_ASSIGN(ProtoExtensionValidator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_ENGINE_PROTO_EXTENSION_VALIDATOR_H_
