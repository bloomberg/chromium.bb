// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chrome/renderer/plugins/plugin_uma.h"

using ::testing::_;

class MockPluginUMASender : public MissingPluginReporter::UMASender {
 public:
  MOCK_METHOD1(SendPluginUMA, void(MissingPluginReporter::PluginType));
};

TEST(PluginUMATest, WindowsMediaPlayer) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock,
              SendPluginUMA(MissingPluginReporter::WINDOWS_MEDIA_PLAYER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "application/x-mplayer2",
    GURL("file://some_file.mov"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "application/x-mplayer2-some_sufix",
    GURL("file://some_file.mov"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "some-prefix-application/x-mplayer2",
    GURL("file://some_file.mov"));
}

TEST(PluginUMATest, Silverlight) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::SILVERLIGHT))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "application/x-silverlight",
    GURL("aaaa"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::SILVERLIGHT))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "application/x-silverlight-some-sufix",
    GURL("aaaa"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "some-prefix-application/x-silverlight",
    GURL("aaaa"));
}

TEST(PluginUMATest, RealPlayer) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::REALPLAYER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "audio/x-pn-realaudio",
    GURL("some url"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::REALPLAYER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "audio/x-pn-realaudio-some-sufix",
    GURL("some url"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "some-prefix-audio/x-pn-realaudio",
    GURL("some url"));
}

TEST(PluginUMATest, Java) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::JAVA))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "application/x-java-applet",
    GURL("some url"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::JAVA))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "application/x-java-applet-some-sufix",
    GURL("some url"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::JAVA))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "some-prefix-application/x-java-applet-sufix",
    GURL("some url"));
}

TEST(PluginUMATest, QuickTime) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::QUICKTIME))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "video/quicktime",
    GURL("some url"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "video/quicktime-sufix",
    GURL("some url"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "prefix-video/quicktime",
    GURL("some url"));
}

TEST(PluginUMATest, BySrcExtension) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::QUICKTIME))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
    "",
    GURL("file://file.mov"));

  // When plugin's mime type is given, we don't check extension.
  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "unknown-plugin",
      GURL("http://file.mov"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "",
      GURL("http://file.unknown_extension"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::QUICKTIME))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "",
      GURL("http://aaa/file.mov?x=aaaa&y=b#c"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::QUICKTIME))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "",
      GURL("http://file.mov?x=aaaa&y=b#c"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "",
      GURL("http://"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::OTHER))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "",
      GURL("mov"));
}

TEST(PluginUMATest, CaseSensitivity) {
  MockPluginUMASender* sender_mock = new MockPluginUMASender();
  MissingPluginReporter::GetInstance()->SetUMASender(sender_mock);
  EXPECT_CALL(*sender_mock, SendPluginUMA(_))
      .Times(0);

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::QUICKTIME))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "video/QUICKTIME",
      GURL("http://file.aaa"));

  EXPECT_CALL(*sender_mock, SendPluginUMA(MissingPluginReporter::QUICKTIME))
      .Times(1)
      .RetiresOnSaturation();
  MissingPluginReporter::GetInstance()->ReportPluginMissing(
      "",
      GURL("http://file.MoV"));
}

