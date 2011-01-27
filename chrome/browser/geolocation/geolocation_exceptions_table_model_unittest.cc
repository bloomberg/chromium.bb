// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/common/content_settings_helper.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kUrl0("http://www.example.com");
const GURL kUrl1("http://www.example1.com");
const GURL kUrl2("http://www.example2.com");

class GeolocationExceptionsTableModelTest : public RenderViewHostTestHarness {
 public:
  GeolocationExceptionsTableModelTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

  virtual ~GeolocationExceptionsTableModelTest() {}

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    ResetModel();
  }

  virtual void TearDown() {
    model_.reset(NULL);
    RenderViewHostTestHarness::TearDown();
  }

  virtual void ResetModel() {
    model_.reset(new GeolocationExceptionsTableModel(
        profile()->GetGeolocationContentSettingsMap()));
  }

  void CreateAllowedSamples() {
    scoped_refptr<GeolocationContentSettingsMap> map(
        profile()->GetGeolocationContentSettingsMap());
    map->SetContentSetting(kUrl0, kUrl0, CONTENT_SETTING_ALLOW);
    map->SetContentSetting(kUrl0, kUrl1, CONTENT_SETTING_ALLOW);
    map->SetContentSetting(kUrl0, kUrl2, CONTENT_SETTING_ALLOW);
    ResetModel();
    EXPECT_EQ(3, model_->RowCount());
  }

 protected:
  BrowserThread ui_thread_;
  scoped_ptr<GeolocationExceptionsTableModel> model_;
};

TEST_F(GeolocationExceptionsTableModelTest, CanRemoveException) {
  EXPECT_EQ(0, model_->RowCount());

  scoped_refptr<GeolocationContentSettingsMap> map(
      profile()->GetGeolocationContentSettingsMap());

  // Ensure a single entry can be removed.
  map->SetContentSetting(kUrl0, kUrl0, CONTENT_SETTING_ALLOW);
  ResetModel();
  EXPECT_EQ(1, model_->RowCount());
  GeolocationExceptionsTableModel::Rows rows;
  rows.insert(0U);
  EXPECT_TRUE(model_->CanRemoveRows(rows));


  // Ensure an entry with children can't be removed.
  map->SetContentSetting(kUrl0, kUrl0, CONTENT_SETTING_DEFAULT);
  map->SetContentSetting(kUrl0, kUrl1, CONTENT_SETTING_ALLOW);
  map->SetContentSetting(kUrl0, kUrl2, CONTENT_SETTING_BLOCK);
  ResetModel();
  EXPECT_EQ(3, model_->RowCount());
  EXPECT_FALSE(model_->CanRemoveRows(rows));

  // Ensure it can be removed if removing all children.
  rows.clear();
  rows.insert(1U);
  rows.insert(2U);
  EXPECT_TRUE(model_->CanRemoveRows(rows));
}

TEST_F(GeolocationExceptionsTableModelTest, RemoveExceptions) {
  CreateAllowedSamples();
  scoped_refptr<GeolocationContentSettingsMap> map(
      profile()->GetGeolocationContentSettingsMap());

  // Test removing parent exception.
  GeolocationExceptionsTableModel::Rows rows;
  rows.insert(0U);
  model_->RemoveRows(rows);
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl0));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, map->GetContentSetting(kUrl0, kUrl1));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, map->GetContentSetting(kUrl0, kUrl2));

  ResetModel();
  EXPECT_EQ(3, model_->RowCount());

  // Test removing remaining children.
  rows.clear();
  rows.insert(1U);
  rows.insert(2U);
  model_->RemoveRows(rows);
  EXPECT_EQ(0, model_->RowCount());
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl0));
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl1));
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl2));
}

TEST_F(GeolocationExceptionsTableModelTest, RemoveAll) {
  CreateAllowedSamples();
  scoped_refptr<GeolocationContentSettingsMap> map(
      profile()->GetGeolocationContentSettingsMap());

  model_->RemoveAll();
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl0));
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl1));
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(kUrl0, kUrl2));
  EXPECT_EQ(0, model_->RowCount());
}

TEST_F(GeolocationExceptionsTableModelTest, GetText) {
  CreateAllowedSamples();

  // Ensure the parent doesn't have any indentation.
  string16 text = model_->GetText(0, IDS_EXCEPTIONS_HOSTNAME_HEADER);
  EXPECT_EQ(content_settings_helper::OriginToString16(kUrl0), text);

  // Ensure there's some indentation on the children nodes.
  text = model_->GetText(1, IDS_EXCEPTIONS_HOSTNAME_HEADER);
  EXPECT_NE(content_settings_helper::OriginToString16(kUrl1), text);
  EXPECT_NE(string16::npos,
            text.find(content_settings_helper::OriginToString16(kUrl1)));

  text = model_->GetText(2, IDS_EXCEPTIONS_HOSTNAME_HEADER);
  EXPECT_NE(content_settings_helper::OriginToString16(kUrl2), text);
  EXPECT_NE(string16::npos,
            text.find(content_settings_helper::OriginToString16(kUrl2)));
}

}  // namespace
