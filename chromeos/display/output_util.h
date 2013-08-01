// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_OUTPUT_UTIL_H_
#define CHROMEOS_DISPLAY_OUTPUT_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef unsigned long XID;
typedef XID RRMode;
struct _XRRModeInfo;
typedef _XRRModeInfo XRRModeInfo;
struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;
struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;

namespace chromeos {

// Gets the EDID data from |output| and generates the display id through
// |GetDisplayIdFromEDID|.
CHROMEOS_EXPORT bool GetDisplayId(XID output, size_t index,
                                  int64* display_id_out);

// Generates the display id for the pair of |prop| with |nitems| length and
// |index|, and store in |display_id_out|. Returns true if the display id is
// successfully generated, or false otherwise.
CHROMEOS_EXPORT bool GetDisplayIdFromEDID(const unsigned char* prop,
                                          unsigned long nitems,
                                          size_t index,
                                          int64* display_id_out);

// Generates the human readable string from EDID obtained for |output|.
CHROMEOS_EXPORT std::string GetDisplayName(XID output);

// Gets the overscan flag from |output| and stores to |flag|. Returns true if
// the flag is found. Otherwise returns false and doesn't touch |flag|. The
// output will produce overscan if |flag| is set to true, but the output may
// still produce overscan even though it returns true and |flag| is set to
// false.
CHROMEOS_EXPORT bool GetOutputOverscanFlag(XID output, bool* flag);

// Parses |prop| as EDID data and stores extracted data into |manufacturer_id|
// and |human_readable_name| and returns true. NULL can be passed for unwanted
// output parameters. Some devices (especially internal displays) may not have
// the field for |human_readable_name|, and it will return true in that case.
CHROMEOS_EXPORT bool ParseOutputDeviceData(const unsigned char* prop,
                                           unsigned long nitems,
                                           uint16* manufacturer_id,
                                           std::string* human_readable_name);

// Parses |prop| as EDID data and stores the overscan flag to |flag|. Returns
// true if the flag is found. This is exported for x11_util_unittest.cc.
CHROMEOS_EXPORT bool ParseOutputOverscanFlag(const unsigned char* prop,
                                             unsigned long nitems,
                                             bool* flag);

// Returns true if an output named |name| is an internal display.
CHROMEOS_EXPORT bool IsInternalOutputName(const std::string& name);

// Find a XRRModeInfo that matches |mode|.
CHROMEOS_EXPORT const XRRModeInfo* FindModeInfo(
    const XRRScreenResources* screen_resources,
    XID mode);

// Find a mode that matches the given size with highest refresh
// rate. Non-interlaced mode takes precedence, so non-interlaced mode
// with a lower refresh rate will be used even if there is an interlaced
// mode with a higher refresh rate.
CHROMEOS_EXPORT RRMode FindOutputModeMatchingSize(
    const XRRScreenResources* screen_resources,
    const XRROutputInfo* output_info,
    size_t width,
    size_t height);

namespace test {

// Creates XRRModeInfo for unit tests.
CHROMEOS_EXPORT XRRModeInfo CreateModeInfo(int id,
                                           int width,
                                           int height,
                                           bool interlaced,
                                           float refresh_rate);

}  // namespace test

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_OUTPUT_UTIL_H_
