// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Pointee;
using testing::Property;

class HostZoomMapTest : public testing::Test {
 public:
  static const double kZoomLevel;
  static const double kDefaultZoomLevel;
  HostZoomMapTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        prefs_(profile_.GetPrefs()),
        per_host_zoom_levels_pref_(prefs::kPerHostZoomLevels),
        url_("http://example.com/test"),
        host_("example.com") {}

 protected:
  void SetPrefObserverExpectation() {
    EXPECT_CALL(
        pref_observer_,
        Observe(NotificationType(NotificationType::PREF_CHANGED),
                _,
                Property(&Details<std::string>::ptr,
                         Pointee(per_host_zoom_levels_pref_))));
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  TestingProfile profile_;
  PrefService* prefs_;
  std::string per_host_zoom_levels_pref_;  // For the observe matcher.
  GURL url_;
  std::string host_;
  NotificationObserverMock pref_observer_;
};
const double HostZoomMapTest::kZoomLevel = 4;
const double HostZoomMapTest::kDefaultZoomLevel = -2;

TEST_F(HostZoomMapTest, LoadNoPrefs) {
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  EXPECT_EQ(0, map->GetZoomLevel(url_));
}

TEST_F(HostZoomMapTest, Load) {
  DictionaryValue* dict =
      prefs_->GetMutableDictionary(prefs::kPerHostZoomLevels);
  dict->SetWithoutPathExpansion(host_, Value::CreateRealValue(kZoomLevel));
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  EXPECT_EQ(kZoomLevel, map->GetZoomLevel(url_));
}

TEST_F(HostZoomMapTest, SetZoomLevel) {
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  PrefChangeRegistrar registrar;
  registrar.Init(prefs_);
  registrar.Add(prefs::kPerHostZoomLevels, &pref_observer_);
  SetPrefObserverExpectation();
  map->SetZoomLevel(url_, kZoomLevel);
  EXPECT_EQ(kZoomLevel, map->GetZoomLevel(url_));
  const DictionaryValue* dict =
      prefs_->GetDictionary(prefs::kPerHostZoomLevels);
  double zoom_level = 0;
  EXPECT_TRUE(dict->GetRealWithoutPathExpansion(host_, &zoom_level));
  EXPECT_EQ(kZoomLevel, zoom_level);

  SetPrefObserverExpectation();
  map->SetZoomLevel(url_, 0);
  EXPECT_EQ(0, map->GetZoomLevel(url_));
  EXPECT_FALSE(dict->HasKey(host_));
}

TEST_F(HostZoomMapTest, ResetToDefaults) {
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  map->SetZoomLevel(url_, kZoomLevel);

  PrefChangeRegistrar registrar;
  registrar.Init(prefs_);
  registrar.Add(prefs::kPerHostZoomLevels, &pref_observer_);
  SetPrefObserverExpectation();
  map->ResetToDefaults();
  EXPECT_EQ(0, map->GetZoomLevel(url_));
  DictionaryValue empty;
  EXPECT_TRUE(
      Value::Equals(&empty, prefs_->GetDictionary(prefs::kPerHostZoomLevels)));
}

TEST_F(HostZoomMapTest, ReloadOnPrefChange) {
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  map->SetZoomLevel(url_, kZoomLevel);

  DictionaryValue dict;
  dict.SetWithoutPathExpansion(host_, Value::CreateRealValue(0));
  prefs_->Set(prefs::kPerHostZoomLevels, dict);
  EXPECT_EQ(0, map->GetZoomLevel(url_));
}

TEST_F(HostZoomMapTest, NoHost) {
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  GURL file_url1_("file:///tmp/test.html");
  GURL file_url2_("file:///tmp/other.html");
  map->SetZoomLevel(file_url1_, kZoomLevel);

  EXPECT_EQ(kZoomLevel, map->GetZoomLevel(file_url1_));
  EXPECT_EQ(0, map->GetZoomLevel(file_url2_));
}

TEST_F(HostZoomMapTest, ChangeDefaultZoomLevel) {
  FundamentalValue zoom_level(kDefaultZoomLevel);
  prefs_->Set(prefs::kDefaultZoomLevel, zoom_level);
  scoped_refptr<HostZoomMap> map(new HostZoomMap(&profile_));
  EXPECT_EQ(kDefaultZoomLevel, map->GetZoomLevel(url_));
}
