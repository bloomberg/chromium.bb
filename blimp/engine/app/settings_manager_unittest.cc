// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/settings_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {

namespace {

class MockSettingsManagerObserver : public SettingsManager::Observer {
 public:
  MOCK_METHOD0(OnWebPreferencesChanged, void());
};

class SettingsManagerTest : public testing::Test {
 public:
  SettingsManagerTest() {}

  void SetUp() override { settings_manager_.AddObserver(&observer_); }

  void TearDown() override { settings_manager_.RemoveObserver(&observer_); }

 protected:
  SettingsManager settings_manager_;
  MockSettingsManagerObserver observer_;
};

TEST_F(SettingsManagerTest, InformsObserversCorrectly) {
  EngineSettings settings = settings_manager_.GetEngineSettings();
  EXPECT_FALSE(settings.record_whole_document);

  EXPECT_CALL(observer_, OnWebPreferencesChanged()).Times(1);

  settings.record_whole_document = true;
  settings_manager_.UpdateEngineSettings(settings);
}

}  // namespace

}  // namespace engine
}  // namespace blimp
