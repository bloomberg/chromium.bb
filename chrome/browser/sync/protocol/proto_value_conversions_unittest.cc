// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "chrome/browser/sync/protocol/proto_value_conversions.h"

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/encryption.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class ProtoValueConversionsTest : public testing::Test {
 protected:
  template <class T>
  void TestSpecificsToValue(
      DictionaryValue* (*specifics_to_value)(const T&)) {
    const T& specifics(T::default_instance());
    scoped_ptr<DictionaryValue> value(specifics_to_value(specifics));
    // We can't do much but make sure that the returned value has
    // something in it.
    EXPECT_FALSE(value->empty());
  }
};

TEST_F(ProtoValueConversionsTest, ProtoChangeCheck) {
  // If this number changes, that means we added or removed a data
  // type.  Don't forget to add a unit test for {New
  // type}SpecificsToValue below.
  EXPECT_EQ(13, syncable::MODEL_TYPE_COUNT);

  // We'd also like to check if we changed any field in our messages.
  // However, that's hard to do: sizeof could work, but it's
  // platform-dependent.  default_instance().ByteSize() won't change
  // for most changes, since most of our fields are optional.  So we
  // just settle for comments in the proto files.
}

TEST_F(ProtoValueConversionsTest, EncryptedDataToValue) {
  TestSpecificsToValue(EncryptedDataToValue);
}

TEST_F(ProtoValueConversionsTest, SessionHeaderToValue) {
  TestSpecificsToValue(SessionHeaderToValue);
}

TEST_F(ProtoValueConversionsTest, SessionTabToValue) {
  TestSpecificsToValue(SessionTabToValue);
}

TEST_F(ProtoValueConversionsTest, SessionWindowToValue) {
  TestSpecificsToValue(SessionWindowToValue);
}

TEST_F(ProtoValueConversionsTest, TabNavigationToValue) {
  TestSpecificsToValue(TabNavigationToValue);
}

TEST_F(ProtoValueConversionsTest, PasswordSpecificsData) {
  sync_pb::PasswordSpecificsData specifics;
  specifics.set_password_value("secret");
  scoped_ptr<DictionaryValue> value(PasswordSpecificsDataToValue(specifics));
  EXPECT_FALSE(value->empty());
  std::string password_value;
  EXPECT_TRUE(value->GetString("password_value", &password_value));
  EXPECT_EQ("<redacted>", password_value);
}

TEST_F(ProtoValueConversionsTest, AppSpecificsToValue) {
  TestSpecificsToValue(AppSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, AutofillSpecificsToValue) {
  TestSpecificsToValue(AutofillSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, AutofillCreditCardSpecificsToValue) {
  TestSpecificsToValue(AutofillCreditCardSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, AutofillProfileSpecificsToValue) {
  TestSpecificsToValue(AutofillProfileSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, BookmarkSpecificsToValue) {
  TestSpecificsToValue(BookmarkSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ExtensionSpecificsToValue) {
  TestSpecificsToValue(ExtensionSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, NigoriSpecificsToValue) {
  TestSpecificsToValue(NigoriSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, PasswordSpecificsToValue) {
  TestSpecificsToValue(PasswordSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, PreferenceSpecificsToValue) {
  TestSpecificsToValue(PreferenceSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, SessionSpecificsToValue) {
  TestSpecificsToValue(SessionSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, ThemeSpecificsToValue) {
  TestSpecificsToValue(ThemeSpecificsToValue);
}

TEST_F(ProtoValueConversionsTest, TypedUrlSpecificsToValue) {
  TestSpecificsToValue(TypedUrlSpecificsToValue);
}

// TODO(akalin): Figure out how to better test EntitySpecificsToValue.

TEST_F(ProtoValueConversionsTest, EntitySpecificsToValue) {
  sync_pb::EntitySpecifics specifics;
  // Touch the extensions to make sure it shows up in the generated
  // value.
#define SET_EXTENSION(key) (void)specifics.MutableExtension(sync_pb::key)

  SET_EXTENSION(app);
  SET_EXTENSION(autofill);
  SET_EXTENSION(autofill_profile);
  SET_EXTENSION(bookmark);
  SET_EXTENSION(extension);
  SET_EXTENSION(nigori);
  SET_EXTENSION(password);
  SET_EXTENSION(preference);
  SET_EXTENSION(session);
  SET_EXTENSION(theme);
  SET_EXTENSION(typed_url);

#undef SET_EXTENSION

  scoped_ptr<DictionaryValue> value(EntitySpecificsToValue(specifics));
  EXPECT_EQ(syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE,
            static_cast<int>(value->size()));
}

}  // namespace
}  // namespace browser_sync
