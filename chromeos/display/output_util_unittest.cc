// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_util.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <X11/extensions/Xrandr.h>

namespace chromeos {

namespace {

// Returns the number of characters in the string literal but doesn't count its
// terminator NULL byte.
#define charsize(str) (arraysize(str) - 1)

// Sample EDID data extracted from real devices.
const unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";

const unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";

const unsigned char kOverscanDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x2d\xfe\x08\x00\x00\x00\x00"
    "\x29\x15\x01\x03\x80\x10\x09\x78\x0a\xee\x91\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\xbd\xef\x80\x71\x4f\x81\xc0\x81\x00\x81\x80\x95\x00"
    "\xa9\xc0\xb3\x00\x01\x01\x02\x3a\x80\x18\x71\x38\x2d\x40\x58\x2c"
    "\x45\x00\xa0\x5a\x00\x00\x00\x1e\x66\x21\x56\xaa\x51\x00\x1e\x30"
    "\x46\x8f\x33\x00\xa0\x5a\x00\x00\x00\x1e\x00\x00\x00\xfd\x00\x18"
    "\x4b\x0f\x51\x17\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
    "\x00\x53\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x01\x1d"
    "\x02\x03\x1f\xf1\x47\x90\x04\x05\x03\x20\x22\x07\x23\x09\x07\x07"
    "\x83\x01\x00\x00\xe2\x00\x0f\x67\x03\x0c\x00\x20\x00\xb8\x2d\x01"
    "\x1d\x80\x18\x71\x1c\x16\x20\x58\x2c\x25\x00\xa0\x5a\x00\x00\x00"
    "\x9e\x01\x1d\x00\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\xa0\x5a\x00"
    "\x00\x00\x1e\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\xa0"
    "\x5a\x00\x00\x00\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc6";

// The EDID info misdetecting overscan once. see crbug.com/226318
const unsigned char kMisdetecedDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x10\xac\x64\x40\x4c\x30\x30\x32"
    "\x0c\x15\x01\x03\x80\x40\x28\x78\xea\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\xa5\x4b\x00\x71\x4f\x81\x00\x81\x80\xd1\x00\xa9\x40"
    "\x01\x01\x01\x01\x01\x01\x28\x3c\x80\xa0\x70\xb0\x23\x40\x30\x20"
    "\x36\x00\x81\x91\x21\x00\x00\x1a\x00\x00\x00\xff\x00\x50\x48\x35"
    "\x4e\x59\x31\x33\x4e\x32\x30\x30\x4c\x0a\x00\x00\x00\xfc\x00\x44"
    "\x45\x4c\x4c\x20\x55\x33\x30\x31\x31\x0a\x20\x20\x00\x00\x00\xfd"
    "\x00\x31\x56\x1d\x5e\x12\x00\x0a\x20\x20\x20\x20\x20\x20\x01\x38"
    "\x02\x03\x29\xf1\x50\x90\x05\x04\x03\x02\x07\x16\x01\x06\x11\x12"
    "\x15\x13\x14\x1f\x20\x23\x0d\x7f\x07\x83\x0f\x00\x00\x67\x03\x0c"
    "\x00\x10\x00\x38\x2d\xe3\x05\x03\x01\x02\x3a\x80\x18\x71\x38\x2d"
    "\x40\x58\x2c\x45\x00\x81\x91\x21\x00\x00\x1e\x01\x1d\x80\x18\x71"
    "\x1c\x16\x20\x58\x2c\x25\x00\x81\x91\x21\x00\x00\x9e\x01\x1d\x00"
    "\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\x81\x91\x21\x00\x00\x1e\x8c"
    "\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\x81\x91\x21\x00\x00"
    "\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x94";

}

const unsigned char kLP2565A[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00"
    "\xA9\x40\xB3\x00\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\xA4";

const unsigned char kLP2565B[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x6E\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x71\x00\xA9\x00\xA9\x40\xA9\x4F"
    "\xB3\x00\xD1\xC0\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x15\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\x45";

TEST(OutputUtilTest, ParseEDID) {
  uint16 manufacturer_id = 0;
  std::string human_readable_name;
  EXPECT_TRUE(ParseOutputDeviceData(
      kNormalDisplay, charsize(kNormalDisplay),
      &manufacturer_id, &human_readable_name));
  EXPECT_EQ(0x22f0u, manufacturer_id);
  EXPECT_EQ("HP ZR30w", human_readable_name);

  manufacturer_id = 0;
  human_readable_name.clear();
  EXPECT_TRUE(ParseOutputDeviceData(
      kInternalDisplay, charsize(kInternalDisplay),
      &manufacturer_id, NULL));
  EXPECT_EQ(0x4ca3u, manufacturer_id);
  EXPECT_EQ("", human_readable_name);

  // Internal display doesn't have name.
  EXPECT_TRUE(ParseOutputDeviceData(
      kInternalDisplay, charsize(kInternalDisplay),
      NULL, &human_readable_name));
  EXPECT_TRUE(human_readable_name.empty());

  manufacturer_id = 0;
  human_readable_name.clear();
  EXPECT_TRUE(ParseOutputDeviceData(
      kOverscanDisplay, charsize(kOverscanDisplay),
      &manufacturer_id, &human_readable_name));
  EXPECT_EQ(0x4c2du, manufacturer_id);
  EXPECT_EQ("SAMSUNG", human_readable_name);
}

TEST(OutputUtilTest, ParseBrokenEDID) {
  uint16 manufacturer_id = 0;
  std::string human_readable_name;

  // length == 0
  EXPECT_FALSE(ParseOutputDeviceData(
      kNormalDisplay, 0,
      &manufacturer_id, &human_readable_name));

  // name is broken. Copying kNormalDisplay and substitute its name data by
  // some control code.
  std::string display_data(
      reinterpret_cast<const char*>(kNormalDisplay), charsize(kNormalDisplay));

  // display's name data is embedded in byte 95-107 in this specific example.
  // Fix here too when the contents of kNormalDisplay is altered.
  display_data[97] = '\x1b';
  EXPECT_FALSE(ParseOutputDeviceData(
      reinterpret_cast<const unsigned char*>(display_data.data()),
      display_data.size(),
      &manufacturer_id, &human_readable_name));

  // If |human_readable_name| isn't specified, it skips parsing the name.
  manufacturer_id = 0;
  EXPECT_TRUE(ParseOutputDeviceData(
      reinterpret_cast<const unsigned char*>(display_data.data()),
      display_data.size(),
      &manufacturer_id, NULL));
  EXPECT_EQ(0x22f0u, manufacturer_id);
}

TEST(OutputUtilTest, ParseOverscanFlag) {
  bool flag = false;
  EXPECT_FALSE(ParseOutputOverscanFlag(
      kNormalDisplay, charsize(kNormalDisplay), &flag));

  flag = false;
  EXPECT_FALSE(ParseOutputOverscanFlag(
      kInternalDisplay, charsize(kInternalDisplay), &flag));

  flag = false;
  EXPECT_TRUE(ParseOutputOverscanFlag(
      kOverscanDisplay, charsize(kOverscanDisplay), &flag));
  EXPECT_TRUE(flag);

  flag = false;
  EXPECT_FALSE(ParseOutputOverscanFlag(
      kMisdetecedDisplay, charsize(kMisdetecedDisplay), &flag));

  flag = false;
  // Copy |kOverscanDisplay| and set flags to false in it. The overscan flags
  // are embedded at byte 150 in this specific example. Fix here too when the
  // contents of kOverscanDisplay is altered.
  std::string display_data(reinterpret_cast<const char*>(kOverscanDisplay),
                           charsize(kOverscanDisplay));
  display_data[150] = '\0';
  EXPECT_TRUE(ParseOutputOverscanFlag(
      reinterpret_cast<const unsigned char*>(display_data.data()),
      display_data.size(), &flag));
  EXPECT_FALSE(flag);
}

TEST(OutputUtilTest, ParseBrokenOverscanData) {
  // Do not fill valid data here because it anyway fails to parse the data.
  scoped_ptr<unsigned char[]> data(new unsigned char[126]);
  bool flag = false;
  EXPECT_FALSE(ParseOutputOverscanFlag(data.get(), 0, &flag));
  EXPECT_FALSE(ParseOutputOverscanFlag(data.get(), 126, &flag));

  // extending data because ParseOutputOverscanFlag() will access the data.
  data.reset(new unsigned char[150]);
  // The number of CEA extensions is stored at byte 126.
  data[126] = '\x01';
  EXPECT_FALSE(ParseOutputOverscanFlag(data.get(), 128, &flag));
  EXPECT_FALSE(ParseOutputOverscanFlag(data.get(), 150, &flag));
}

TEST(OutputUtilTest, IsInternalOutputName) {
  EXPECT_TRUE(IsInternalOutputName("LVDS"));
  EXPECT_TRUE(IsInternalOutputName("eDP"));
  EXPECT_TRUE(IsInternalOutputName("DSI"));
  EXPECT_TRUE(IsInternalOutputName("LVDSxx"));
  EXPECT_TRUE(IsInternalOutputName("eDPzz"));
  EXPECT_TRUE(IsInternalOutputName("DSIyy"));

  EXPECT_FALSE(IsInternalOutputName("xyz"));
  EXPECT_FALSE(IsInternalOutputName("abcLVDS"));
  EXPECT_FALSE(IsInternalOutputName("cdeeDP"));
  EXPECT_FALSE(IsInternalOutputName("abcDSI"));
  EXPECT_FALSE(IsInternalOutputName("LVD"));
  EXPECT_FALSE(IsInternalOutputName("eD"));
  EXPECT_FALSE(IsInternalOutputName("DS"));
}

TEST(OutputUtilTest, GetDisplayId) {
  // EDID of kLP2565A and B are slightly different but actually the same device.
  int64 id1 = -1;
  int64 id2 = -1;
  EXPECT_TRUE(GetDisplayIdFromEDID(kLP2565A, charsize(kLP2565A), 0, &id1));
  EXPECT_TRUE(GetDisplayIdFromEDID(kLP2565B, charsize(kLP2565B), 0, &id2));
  EXPECT_EQ(id1, id2);
  EXPECT_NE(-1, id1);
}

TEST(OutputUtilTest, GetDisplayIdFromInternal) {
  int64 id = -1;
  EXPECT_TRUE(GetDisplayIdFromEDID(
      kInternalDisplay, charsize(kInternalDisplay), 0, &id));
  EXPECT_NE(-1, id);
}

TEST(OutputUtilTest, GetDisplayIdFailure) {
  int64 id = -1;
  EXPECT_FALSE(GetDisplayIdFromEDID(NULL, 0, 0, &id));
  EXPECT_EQ(-1, id);
}

TEST(OutputUtilTest, FindOutputModeMatchingSize) {
  XRRScreenResources resources = {0};
  RROutput outputs[] = {1};
  resources.noutput = arraysize(outputs);
  resources.outputs = outputs;
  XRRModeInfo modes[] = {
    // id, width, height, interlaced, refresh rate
    test::CreateModeInfo(11, 1920, 1200, false, 60.0f),

    // different rates
    test::CreateModeInfo(12, 1920, 1080, false, 30.0f),
    test::CreateModeInfo(13, 1920, 1080, false, 50.0f),
    test::CreateModeInfo(14, 1920, 1080, false, 40.0f),
    test::CreateModeInfo(15, 1920, 1080, false, 0.0f),

    // interlace vs non interlace
    test::CreateModeInfo(16, 1280, 720, true, 60.0f),
    test::CreateModeInfo(17, 1280, 720, false, 40.0f),

    // interlace only
    test::CreateModeInfo(18, 1024, 768, true, 0.0f),
    test::CreateModeInfo(19, 1024, 768, true, 40.0f),
    test::CreateModeInfo(20, 1024, 768, true, 60.0f),

    // mixed
    test::CreateModeInfo(21, 1024, 600, true, 60.0f),
    test::CreateModeInfo(22, 1024, 600, false, 40.0f),
    test::CreateModeInfo(23, 1024, 600, false, 50.0f),

    // just one interlaced mode
    test::CreateModeInfo(24, 640, 480, true, 60.0f),

    // refresh rate not available.
    test::CreateModeInfo(25, 320, 200, false, 0.0f),
  };
  resources.nmode = arraysize(modes);
  resources.modes = modes;

  XRROutputInfo output_info = {0};
  RRMode output_modes[] = {
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  };
  output_info.nmode = arraysize(output_modes);
  output_info.modes = output_modes;

  EXPECT_EQ(11u, FindOutputModeMatchingSize(&resources,
                                           &output_info,
                                           1920, 1200));

  // Should pick highest refresh rate.
  EXPECT_EQ(13u, FindOutputModeMatchingSize(&resources,
                                            &output_info,
                                            1920, 1080));

  // Should pick non interlaced mode.
  EXPECT_EQ(17u, FindOutputModeMatchingSize(&resources,
                                            &output_info,
                                            1280, 720));

  // Interlaced only. Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(20u, FindOutputModeMatchingSize(&resources,
                                            &output_info,
                                            1024, 768));

  // Mixed: Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(23u, FindOutputModeMatchingSize(&resources,
                                            &output_info,
                                            1024, 600));

  // Just one interlaced mode.
  EXPECT_EQ(24u, FindOutputModeMatchingSize(&resources,
                                            &output_info,
                                            640, 480));

  // Refresh rate not available.
  EXPECT_EQ(25u, FindOutputModeMatchingSize(&resources,
                                            &output_info,
                                            320, 200));

  // No mode found.
  EXPECT_EQ(static_cast<XID>(None),
            FindOutputModeMatchingSize(&resources,
                                       &output_info,
                                       1440, 900));
}

}   // namespace chromeos
