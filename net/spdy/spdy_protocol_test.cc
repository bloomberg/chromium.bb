// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "testing/platform_test.h"

namespace {

enum SpdyProtocolTestTypes {
  SPDY2 = 2,
  SPDY3 = 3,
};

} // namespace

namespace net {

class SpdyProtocolTest
    : public ::testing::TestWithParam<SpdyProtocolTestTypes> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
  }

  bool IsSpdy2() { return spdy_version_ == SPDY2; }

  // Version of SPDY protocol to be used.
  int spdy_version_;
};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolTests,
                        SpdyProtocolTest,
                        ::testing::Values(SPDY2, SPDY3));

// Test our protocol constants
TEST_P(SpdyProtocolTest, ProtocolConstants) {
  EXPECT_EQ(8u, SpdyFrame::kHeaderSize);
  EXPECT_EQ(8u, SpdyDataFrame::size());
  EXPECT_EQ(8u, SpdyControlFrame::kHeaderSize);
  EXPECT_EQ(12u, SpdySettingsControlFrame::size());
  EXPECT_EQ(4u, sizeof(FlagsAndLength));
  EXPECT_EQ(1, SYN_STREAM);
  EXPECT_EQ(2, SYN_REPLY);
  EXPECT_EQ(3, RST_STREAM);
  EXPECT_EQ(4, SETTINGS);
  EXPECT_EQ(5, NOOP);
  EXPECT_EQ(6, PING);
  EXPECT_EQ(7, GOAWAY);
  EXPECT_EQ(8, HEADERS);
  EXPECT_EQ(9, WINDOW_UPDATE);
}

// Test some of the protocol helper functions
TEST_P(SpdyProtocolTest, FrameStructs) {
  SpdyFrame frame(SpdyFrame::kHeaderSize);
  frame.set_length(12345);
  frame.set_flags(10);
  EXPECT_EQ(12345u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_FALSE(frame.is_control_frame());

  frame.set_length(0);
  frame.set_flags(10);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_FALSE(frame.is_control_frame());
}

TEST_P(SpdyProtocolTest, DataFrameStructs) {
  SpdyDataFrame data_frame;
  data_frame.set_stream_id(12345);
  EXPECT_EQ(12345u, data_frame.stream_id());
}

TEST_P(SpdyProtocolTest, TestDataFrame) {
  SpdyDataFrame frame;

  // Set the stream ID to various values.
  frame.set_stream_id(0);
  EXPECT_EQ(0u, frame.stream_id());
  EXPECT_FALSE(frame.is_control_frame());
  frame.set_stream_id(~0 & kStreamIdMask);
  EXPECT_EQ(~0 & kStreamIdMask, frame.stream_id());
  EXPECT_FALSE(frame.is_control_frame());

  // Set length to various values.  Make sure that when you set_length(x),
  // length() == x.  Also make sure the flags are unaltered.
  memset(frame.data(), '1', SpdyDataFrame::size());
  int8 flags = frame.flags();
  frame.set_length(0);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(flags, frame.flags());
  frame.set_length(kLengthMask);
  EXPECT_EQ(kLengthMask, frame.length());
  EXPECT_EQ(flags, frame.flags());
  frame.set_length(5u);
  EXPECT_EQ(5u, frame.length());
  EXPECT_EQ(flags, frame.flags());

  // Set flags to various values.  Make sure that when you set_flags(x),
  // flags() == x.  Also make sure the length is unaltered.
  memset(frame.data(), '1', SpdyDataFrame::size());
  uint32 length = frame.length();
  frame.set_flags(0u);
  EXPECT_EQ(0u, frame.flags());
  EXPECT_EQ(length, frame.length());
  int8 all_flags = ~0;
  frame.set_flags(all_flags);
  flags = frame.flags();
  EXPECT_EQ(all_flags, flags);
  EXPECT_EQ(length, frame.length());
  frame.set_flags(5u);
  EXPECT_EQ(5u, frame.flags());
  EXPECT_EQ(length, frame.length());
}

// Test various types of SETTINGS frames.
TEST_P(SpdyProtocolTest, TestSpdySettingsFrame) {
  SpdyFramer framer(spdy_version_);

  // Create a settings frame with no settings.
  SettingsMap settings;
  scoped_ptr<SpdySettingsControlFrame> settings_frame(
      framer.CreateSettings(settings));
  EXPECT_EQ(framer.protocol_version(), settings_frame->version());
  EXPECT_TRUE(settings_frame->is_control_frame());
  EXPECT_EQ(SETTINGS, settings_frame->type());
  EXPECT_EQ(0u, settings_frame->num_entries());

  // We'll add several different ID/Flag combinations and then verify
  // that they encode and decode properly.
  SettingsFlagsAndId ids[] = {
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0x00000000),
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0x00010203),
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0x01030402),
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0x02030401),
    SettingsFlagsAndId(3, 9)
  };

  for (uint32 index = 0; index < arraysize(ids); ++index) {
    SettingsFlagsAndId flags_and_id = ids[index];
    SpdySettingsIds id = static_cast<SpdySettingsIds>(flags_and_id.id());
    SpdySettingsFlags flags =
        static_cast<SpdySettingsFlags>(flags_and_id.flags());
    settings[id] = SettingsFlagsAndValue(flags, index);
    settings_frame.reset(framer.CreateSettings(settings));
    EXPECT_EQ(framer.protocol_version(), settings_frame->version());
    EXPECT_TRUE(settings_frame->is_control_frame());
    EXPECT_EQ(SETTINGS, settings_frame->type());
    EXPECT_EQ(index + 1, settings_frame->num_entries());

    SettingsMap parsed_settings;
    EXPECT_TRUE(framer.ParseSettings(settings_frame.get(), &parsed_settings));
    EXPECT_EQ(settings.size(), parsed_settings.size());
    for (SettingsMap::const_iterator it = parsed_settings.begin();
         it != parsed_settings.end();
         it++) {
      SettingsMap::const_iterator it2 = settings.find(it->first);
      EXPECT_EQ(it->first, it2->first);
      SettingsFlagsAndValue parsed = it->second;
      SettingsFlagsAndValue created = it2->second;
      EXPECT_EQ(created.first, parsed.first);
      EXPECT_EQ(created.second, parsed.second);
    }
  }
}

TEST_P(SpdyProtocolTest, HasHeaderBlock) {
  SpdyControlFrame frame(SpdyControlFrame::kHeaderSize);
  for (SpdyControlType type = SYN_STREAM;
      type < NUM_CONTROL_FRAME_TYPES;
      type = static_cast<SpdyControlType>(type + 1)) {
    frame.set_type(type);
    if (type == SYN_STREAM || type == SYN_REPLY || type == HEADERS) {
      EXPECT_TRUE(frame.has_header_block());
    } else {
      EXPECT_FALSE(frame.has_header_block());
    }
  }
}

class SpdyProtocolDeathTest : public SpdyProtocolTest {};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolDeathTests,
                        SpdyProtocolDeathTest,
                        ::testing::Values(SPDY2, SPDY3));

// Make sure that overflows both die in debug mode, and do not cause problems
// in opt mode.  Note:  The EXPECT_DEBUG_DEATH call does not work on Win32 yet,
// so we comment it out.
TEST_P(SpdyProtocolDeathTest, TestDataFrame) {
  SpdyDataFrame frame;

  frame.set_stream_id(0);
  // TODO(mbelshe):  implement EXPECT_DEBUG_DEATH on windows.
#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(frame.set_stream_id(~0), "");
#else
  EXPECT_DEATH(frame.set_stream_id(~0), "");
#endif
#endif
  EXPECT_FALSE(frame.is_control_frame());

  frame.set_flags(0);
#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(frame.set_length(~0), "");
#else
  EXPECT_DEATH(frame.set_length(~0), "");
#endif
#endif
  EXPECT_EQ(0, frame.flags());
}

TEST_P(SpdyProtocolDeathTest, TestSpdyControlFrameType) {
  SpdyControlFrame frame(SpdyControlFrame::kHeaderSize);
  memset(frame.data(), 255, SpdyControlFrame::kHeaderSize);

  // type() should be out of bounds.
  EXPECT_FALSE(frame.AppearsToBeAValidControlFrame());

  frame.set_version(spdy_version_);
  uint16 version = frame.version();

  for (int i = SYN_STREAM; i <= WINDOW_UPDATE; ++i) {
    frame.set_type(static_cast<SpdyControlType>(i));
    EXPECT_EQ(i, static_cast<int>(frame.type()));
    if (!IsSpdy2() && i == NOOP) {
      // NOOP frames aren't 'valid'.
      EXPECT_FALSE(frame.AppearsToBeAValidControlFrame());
    } else {
      EXPECT_TRUE(frame.AppearsToBeAValidControlFrame());
    }
    // Make sure setting type does not alter the version block.
    EXPECT_EQ(version, frame.version());
    EXPECT_TRUE(frame.is_control_frame());
  }
}

}  // namespace net
