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

}

class WebIntentPickerModelObserverMock : public WebIntentPickerModelObserver {
 public:
  MOCK_METHOD1(OnModelChanged, void(WebIntentPickerModel* model));
  MOCK_METHOD2(OnFaviconChanged,
               void(WebIntentPickerModel* model, size_t index));
  MOCK_METHOD1(OnInlineDisposition, void(WebIntentPickerModel* model));
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

TEST_F(WebIntentPickerModelTest, AddItem) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);

  EXPECT_EQ(0U, model_.GetItemCount());

  model_.AddItem(kTitle1, kUrl1, WebIntentPickerModel::DISPOSITION_WINDOW);
  model_.AddItem(kTitle2, kUrl2, WebIntentPickerModel::DISPOSITION_WINDOW);

  EXPECT_EQ(2U, model_.GetItemCount());
  EXPECT_EQ(kUrl1, model_.GetItemAt(0).url);
  EXPECT_EQ(kUrl2, model_.GetItemAt(1).url);
}

TEST_F(WebIntentPickerModelTest, RemoveItemAt) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(4);

  model_.AddItem(kTitle1, kUrl1, WebIntentPickerModel::DISPOSITION_WINDOW);
  model_.AddItem(kTitle2, kUrl2, WebIntentPickerModel::DISPOSITION_WINDOW);
  model_.AddItem(kTitle3, kUrl3, WebIntentPickerModel::DISPOSITION_WINDOW);

  EXPECT_EQ(3U, model_.GetItemCount());

  model_.RemoveItemAt(1);

  EXPECT_EQ(2U, model_.GetItemCount());
  EXPECT_EQ(kUrl1, model_.GetItemAt(0).url);
  EXPECT_EQ(kUrl3, model_.GetItemAt(1).url);
}

TEST_F(WebIntentPickerModelTest, Clear) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(3);

  model_.AddItem(kTitle1, kUrl1, WebIntentPickerModel::DISPOSITION_WINDOW);
  model_.AddItem(kTitle2, kUrl2, WebIntentPickerModel::DISPOSITION_WINDOW);

  EXPECT_EQ(2U, model_.GetItemCount());

  model_.Clear();

  EXPECT_EQ(0U, model_.GetItemCount());
}

TEST_F(WebIntentPickerModelTest, UpdateFaviconAt) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(2);
  EXPECT_CALL(observer_, OnFaviconChanged(&model_, 1U)).Times(1);

  model_.AddItem(kTitle1, kUrl1, WebIntentPickerModel::DISPOSITION_WINDOW);
  model_.AddItem(kTitle2, kUrl2, WebIntentPickerModel::DISPOSITION_WINDOW);
  gfx::Image image(gfx::test::CreateImage());
  model_.UpdateFaviconAt(1U, image);

  EXPECT_FALSE(gfx::test::IsEqual(image, model_.GetItemAt(0).favicon));
  EXPECT_TRUE(gfx::test::IsEqual(image, model_.GetItemAt(1).favicon));
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

TEST_F(WebIntentPickerModelTest, SetInlineDisposition) {
  EXPECT_CALL(observer_, OnModelChanged(&model_)).Times(3);
  EXPECT_CALL(observer_, OnInlineDisposition(&model_)).Times(1);

  EXPECT_FALSE(model_.IsInlineDisposition());
  EXPECT_EQ(std::string::npos, model_.inline_disposition_index());

  model_.AddItem(kTitle1, kUrl1, WebIntentPickerModel::DISPOSITION_WINDOW);
  model_.AddItem(kTitle2, kUrl2, WebIntentPickerModel::DISPOSITION_INLINE);
  model_.SetInlineDisposition(1);

  EXPECT_TRUE(model_.IsInlineDisposition());
  EXPECT_EQ(1U, model_.inline_disposition_index());

  model_.Clear();

  EXPECT_FALSE(model_.IsInlineDisposition());
  EXPECT_EQ(std::string::npos, model_.inline_disposition_index());
}
