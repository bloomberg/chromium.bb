// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

const string16 kTitle1(ASCIIToUTF16("Foo"));
const string16 kTitle2(ASCIIToUTF16("Bar"));
const string16 kTitle3(ASCIIToUTF16("Baz"));
const GURL kUrl1("http://www.example.com/foo");
const GURL kUrl2("http://www.example.com/bar");
const GURL kUrl3("http://www.example.com/baz");
const string16 kId1(ASCIIToUTF16("nhkckhebbbncbkefhcpcgepcgfaclehe"));
const string16 kId2(ASCIIToUTF16("hcpcgepcgfaclehenhkckhebbbncbkef"));
const string16 kId3(ASCIIToUTF16("aclehenhkckhebbbncbkefhcpcgepcgf"));
const WebIntentPickerModel::Disposition kWindowDisposition(
    WebIntentPickerModel::DISPOSITION_WINDOW);
const WebIntentPickerModel::Disposition kInlineDisposition(
    WebIntentPickerModel::DISPOSITION_INLINE);

}

class WebIntentPickerModelObserverMock : public WebIntentPickerModelObserver {
 public:
  MOCK_METHOD1(OnModelChanged, void(WebIntentPickerModel* model));
  MOCK_METHOD2(OnFaviconChanged,
               void(WebIntentPickerModel* model, size_t index));
  MOCK_METHOD2(OnExtensionIconChanged,
               void(WebIntentPickerModel* model, const string16& extension_id));
  MOCK_METHOD2(OnInlineDisposition,
               void(const string16& title, const GURL& url));
};

class WebIntentPickerModelTest : public testing::Test {
 public:
  WebIntentPickerModelTest() {}

  virtual void SetUp() {
    testing::Test::SetUp();
    model_.set_observer(&observer_);
  }

  WebIntentPickerModel model_;
  WebIntentPickerModelObserverMock observer_;
};

TEST_F(WebIntentPickerModelTest, AddInstalledService) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);

  EXPECT_EQ(0U, model_.GetInstalledServiceCount());

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);

  EXPECT_EQ(2U, model_.GetInstalledServiceCount());
  EXPECT_EQ(kUrl1, model_.GetInstalledServiceAt(0).url);
  EXPECT_EQ(kUrl2, model_.GetInstalledServiceAt(1).url);
}

// Test that AddInstalledServices() is idempotent.
TEST_F(WebIntentPickerModelTest, AddInstalledServiceIsIdempotent) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);

  EXPECT_EQ(0U, model_.GetInstalledServiceCount());

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);

  EXPECT_EQ(2U, model_.GetInstalledServiceCount());
  EXPECT_EQ(kUrl1, model_.GetInstalledServiceAt(0).url);
  EXPECT_EQ(kUrl2, model_.GetInstalledServiceAt(1).url);

  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);
  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);

  EXPECT_EQ(2U, model_.GetInstalledServiceCount());
  EXPECT_EQ(kUrl1, model_.GetInstalledServiceAt(0).url);
  EXPECT_EQ(kUrl2, model_.GetInstalledServiceAt(1).url);
}


TEST_F(WebIntentPickerModelTest, RemoveInstalledServiceAt) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(4);

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);
  model_.AddInstalledService(kTitle3, kUrl3, kWindowDisposition);

  EXPECT_EQ(3U, model_.GetInstalledServiceCount());

  model_.RemoveInstalledServiceAt(1);

  EXPECT_EQ(2U, model_.GetInstalledServiceCount());
  EXPECT_EQ(kUrl1, model_.GetInstalledServiceAt(0).url);
  EXPECT_EQ(kUrl3, model_.GetInstalledServiceAt(1).url);
}

TEST_F(WebIntentPickerModelTest, Clear) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(3);

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);

  EXPECT_EQ(2U, model_.GetInstalledServiceCount());

  model_.Clear();

  EXPECT_EQ(0U, model_.GetInstalledServiceCount());
}

TEST_F(WebIntentPickerModelTest, GetInstalledServiceWithURL) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);

  EXPECT_EQ(kTitle2, model_.GetInstalledServiceWithURL(kUrl2)->title);
  EXPECT_EQ(NULL, model_.GetInstalledServiceWithURL(kUrl3));
}

TEST_F(WebIntentPickerModelTest, UpdateFaviconAt) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);
  EXPECT_CALL(observer_, OnFaviconChanged(&model_, 1U)).Times(1);

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kWindowDisposition);
  gfx::Image image(gfx::test::CreateImage());
  model_.UpdateFaviconAt(1U, image);

  EXPECT_FALSE(gfx::test::IsEqual(
      image, model_.GetInstalledServiceAt(0).favicon));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, model_.GetInstalledServiceAt(1).favicon));
}

TEST_F(WebIntentPickerModelTest, AddSuggestedExtension) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);

  EXPECT_EQ(0U, model_.GetSuggestedExtensionCount());

  model_.AddSuggestedExtension(kTitle1, kId1, 3.0);
  model_.AddSuggestedExtension(kTitle2, kId2, 4.3);

  EXPECT_EQ(2U, model_.GetSuggestedExtensionCount());
  EXPECT_EQ(kId1, model_.GetSuggestedExtensionAt(0).id);
  EXPECT_EQ(kId2, model_.GetSuggestedExtensionAt(1).id);
}

TEST_F(WebIntentPickerModelTest, RemoveSuggestedExtensionAt) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(4);

  model_.AddSuggestedExtension(kTitle1, kId1, 3.0);
  model_.AddSuggestedExtension(kTitle2, kId2, 4.3);
  model_.AddSuggestedExtension(kTitle3, kId3, 1.6);

  EXPECT_EQ(3U, model_.GetSuggestedExtensionCount());

  model_.RemoveSuggestedExtensionAt(1);

  EXPECT_EQ(2U, model_.GetSuggestedExtensionCount());
  EXPECT_EQ(kId1, model_.GetSuggestedExtensionAt(0).id);
  EXPECT_EQ(kId3, model_.GetSuggestedExtensionAt(1).id);
}

TEST_F(WebIntentPickerModelTest, SetSuggestedExtensionIconWithId) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);
  EXPECT_CALL(observer_, OnExtensionIconChanged(&model_, kId2)).Times(1);

  model_.AddSuggestedExtension(kTitle1, kId1, 3.0);
  model_.AddSuggestedExtension(kTitle2, kId2, 4.3);

  gfx::Image image(gfx::test::CreateImage());
  model_.SetSuggestedExtensionIconWithId(kId2, image);

  EXPECT_FALSE(gfx::test::IsEqual(
      image, model_.GetSuggestedExtensionAt(0).icon));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, model_.GetSuggestedExtensionAt(1).icon));
}

TEST_F(WebIntentPickerModelTest, SetInlineDisposition) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(3);
  EXPECT_CALL(observer_, OnInlineDisposition(kTitle2, testing::_)).Times(1);

  EXPECT_FALSE(model_.IsInlineDisposition());
  EXPECT_EQ(GURL::EmptyGURL(), model_.inline_disposition_url());

  model_.AddInstalledService(kTitle1, kUrl1, kWindowDisposition);
  model_.AddInstalledService(kTitle2, kUrl2, kInlineDisposition);
  model_.SetInlineDisposition(kUrl2);

  EXPECT_TRUE(model_.IsInlineDisposition());
  EXPECT_EQ(kUrl2, model_.inline_disposition_url());

  model_.Clear();

  EXPECT_FALSE(model_.IsInlineDisposition());
  EXPECT_EQ(GURL::EmptyGURL(), model_.inline_disposition_url());
}
