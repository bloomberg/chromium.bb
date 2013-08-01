// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_util.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>

#include "base/hash.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"

namespace chromeos {
namespace {

// Prefixes for the built-in displays.
const char kInternal_LVDS[] = "LVDS";
const char kInternal_eDP[] = "eDP";
const char kInternal_DSI[] = "DSI";

// Returns 64-bit persistent ID for the specified manufacturer's ID and
// product_code_hash, and the index of the output it is connected to.
// |output_index| is used to distinguish the displays of the same type. For
// example, swapping two identical display between two outputs will not be
// treated as swap. The 'serial number' field in EDID isn't used here because
// it is not guaranteed to have unique number and it may have the same fixed
// value (like 0).
int64 GetID(uint16 manufacturer_id,
            uint32 product_code_hash,
            uint8 output_index) {
  return ((static_cast<int64>(manufacturer_id) << 40) |
          (static_cast<int64>(product_code_hash) << 8) | output_index);
}

bool IsRandRAvailable() {
  int randr_version_major = 0;
  int randr_version_minor = 0;
  static bool is_randr_available = XRRQueryVersion(
      base::MessagePumpAuraX11::GetDefaultXDisplay(),
      &randr_version_major, &randr_version_minor);
  return is_randr_available;
}

// Get the EDID data from the |output| and stores to |prop|. |nitem| will store
// the number of characters |prop| will have. It doesn't take the ownership of
// |prop|, so caller must release it by XFree().
// Returns true if EDID property is successfully obtained. Otherwise returns
// false and does not touch |prop| and |nitems|.
bool GetEDIDProperty(XID output, unsigned long* nitems, unsigned char** prop) {
  if (!IsRandRAvailable())
    return false;

  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();

  static Atom edid_property = XInternAtom(
      base::MessagePumpAuraX11::GetDefaultXDisplay(),
      RR_PROPERTY_RANDR_EDID, false);

  bool has_edid_property = false;
  int num_properties = 0;
  Atom* properties = XRRListOutputProperties(display, output, &num_properties);
  for (int i = 0; i < num_properties; ++i) {
    if (properties[i] == edid_property) {
      has_edid_property = true;
      break;
    }
  }
  XFree(properties);
  if (!has_edid_property)
    return false;

  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;
  XRRGetOutputProperty(display,
                       output,
                       edid_property,
                       0,                // offset
                       128,              // length
                       false,            // _delete
                       false,            // pending
                       AnyPropertyType,  // req_type
                       &actual_type,
                       &actual_format,
                       nitems,
                       &bytes_after,
                       prop);
  DCHECK_EQ(XA_INTEGER, actual_type);
  DCHECK_EQ(8, actual_format);
  return true;
}

// Gets some useful data from the specified output device, such like
// manufacturer's ID, product code, and human readable name. Returns false if it
// fails to get those data and doesn't touch manufacturer ID/product code/name.
// NULL can be passed for unwanted output parameters.
bool GetOutputDeviceData(XID output,
                         uint16* manufacturer_id,
                         std::string* human_readable_name) {
  unsigned long nitems = 0;
  unsigned char *prop = NULL;
  if (!GetEDIDProperty(output, &nitems, &prop))
    return false;

  bool result = ParseOutputDeviceData(
      prop, nitems, manufacturer_id, human_readable_name);
  XFree(prop);
  return result;
}

float GetRefreshRate(const XRRModeInfo* mode_info) {
  if (mode_info->hTotal && mode_info->vTotal) {
    return static_cast<float>(mode_info->dotClock) /
        (static_cast<float>(mode_info->hTotal) *
         static_cast<float>(mode_info->vTotal));
  } else {
    return 0.0f;
  }
}

}  // namespace

std::string GetDisplayName(XID output_id) {
  std::string display_name;
  GetOutputDeviceData(output_id, NULL, &display_name);
  return display_name;
}

bool GetDisplayId(XID output_id, size_t output_index, int64* display_id_out) {
  unsigned long nitems = 0;
  unsigned char* prop = NULL;
  if (!GetEDIDProperty(output_id, &nitems, &prop))
    return false;

  bool result =
      GetDisplayIdFromEDID(prop, nitems, output_index, display_id_out);
  XFree(prop);
  return result;
}

bool GetDisplayIdFromEDID(const unsigned char* prop,
                          unsigned long nitems,
                          size_t output_index,
                          int64* display_id_out) {
  uint16 manufacturer_id = 0;
  std::string product_name;

  // ParseOutputDeviceData fails if it doesn't have product_name.
  ParseOutputDeviceData(prop, nitems, &manufacturer_id, &product_name);

  // Generates product specific value from product_name instead of product code.
  // See crbug.com/240341
  uint32 product_code_hash = product_name.empty() ?
      0 : base::Hash(product_name);
  if (manufacturer_id != 0) {
    // An ID based on display's index will be assigned later if this call
    // fails.
    *display_id_out = GetID(
        manufacturer_id, product_code_hash, output_index);
    return true;
  }
  return false;
}

bool ParseOutputDeviceData(const unsigned char* prop,
                           unsigned long nitems,
                           uint16* manufacturer_id,
                           std::string* human_readable_name) {
  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the details of EDID data format.  We use the following data:
  //   bytes 8-9: manufacturer EISA ID, in big-endian
  //   bytes 54-125: four descriptors (18-bytes each) which may contain
  //     the display name.
  const unsigned int kManufacturerOffset = 8;
  const unsigned int kManufacturerLength = 2;
  const unsigned int kDescriptorOffset = 54;
  const unsigned int kNumDescriptors = 4;
  const unsigned int kDescriptorLength = 18;
  // The specifier types.
  const unsigned char kMonitorNameDescriptor = 0xfc;

  if (manufacturer_id) {
    if (nitems < kManufacturerOffset + kManufacturerLength) {
      LOG(ERROR) << "too short EDID data: manifacturer id";
      return false;
    }

    *manufacturer_id =
        *reinterpret_cast<const uint16*>(prop + kManufacturerOffset);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    *manufacturer_id = base::ByteSwap(*manufacturer_id);
#endif
  }

  if (!human_readable_name)
    return true;

  human_readable_name->clear();
  for (unsigned int i = 0; i < kNumDescriptors; ++i) {
    if (nitems < kDescriptorOffset + (i + 1) * kDescriptorLength)
      break;

    const unsigned char* desc_buf =
        prop + kDescriptorOffset + i * kDescriptorLength;
    // If the descriptor contains the display name, it has the following
    // structure:
    //   bytes 0-2, 4: \0
    //   byte 3: descriptor type, defined above.
    //   bytes 5-17: text data, ending with \r, padding with spaces
    // we should check bytes 0-2 and 4, since it may have other values in
    // case that the descriptor contains other type of data.
    if (desc_buf[0] == 0 && desc_buf[1] == 0 && desc_buf[2] == 0 &&
        desc_buf[4] == 0) {
      if (desc_buf[3] == kMonitorNameDescriptor) {
        std::string found_name(
            reinterpret_cast<const char*>(desc_buf + 5), kDescriptorLength - 5);
        TrimWhitespaceASCII(found_name, TRIM_TRAILING, human_readable_name);
        break;
      }
    }
  }

  // Verify if the |human_readable_name| consists of printable characters only.
  for (size_t i = 0; i < human_readable_name->size(); ++i) {
    char c = (*human_readable_name)[i];
    if (!isascii(c) || !isprint(c)) {
      human_readable_name->clear();
      LOG(ERROR) << "invalid EDID: human unreadable char in name";
      return false;
    }
  }

  return true;
}

bool GetOutputOverscanFlag(XID output, bool* flag) {
  unsigned long nitems = 0;
  unsigned char *prop = NULL;
  if (!GetEDIDProperty(output, &nitems, &prop))
    return false;

  bool found = ParseOutputOverscanFlag(prop, nitems, flag);
  XFree(prop);
  return found;
}

bool ParseOutputOverscanFlag(const unsigned char* prop,
                             unsigned long nitems,
                             bool *flag) {
  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the extension format of EDID.  Also see EIA/CEA-861 spec for
  // the format of the extensions and how video capability is encoded.
  //  - byte 0: tag.  should be 02h.
  //  - byte 1: revision.  only cares revision 3 (03h).
  //  - byte 4-: data block.
  const unsigned int kExtensionBase = 128;
  const unsigned int kExtensionSize = 128;
  const unsigned int kNumExtensionsOffset = 126;
  const unsigned int kDataBlockOffset = 4;
  const unsigned char kCEAExtensionTag = '\x02';
  const unsigned char kExpectedExtensionRevision = '\x03';
  const unsigned char kExtendedTag = 7;
  const unsigned char kExtendedVideoCapabilityTag = 0;
  const unsigned int kPTOverscan = 4;
  const unsigned int kITOverscan = 2;
  const unsigned int kCEOverscan = 0;

  if (nitems <= kNumExtensionsOffset)
    return false;

  unsigned char num_extensions = prop[kNumExtensionsOffset];

  for (size_t i = 0; i < num_extensions; ++i) {
    // Skip parsing the whole extension if size is not enough.
    if (nitems < kExtensionBase + (i + 1) * kExtensionSize)
      break;

    const unsigned char* extension = prop + kExtensionBase + i * kExtensionSize;
    unsigned char tag = extension[0];
    unsigned char revision = extension[1];
    if (tag != kCEAExtensionTag || revision != kExpectedExtensionRevision)
      continue;

    unsigned char timing_descriptors_start =
        std::min(extension[2], static_cast<unsigned char>(kExtensionSize));
    const unsigned char* data_block = extension + kDataBlockOffset;
    while (data_block < extension + timing_descriptors_start) {
      // A data block is encoded as:
      // - byte 1 high 3 bits: tag. '07' for extended tags.
      // - byte 1 remaining bits: the length of data block.
      // - byte 2: the extended tag.  '0' for video capability.
      // - byte 3: the capability.
      unsigned char tag = data_block[0] >> 5;
      unsigned char payload_length = data_block[0] & 0x1f;
      if (static_cast<unsigned long>(data_block + payload_length - prop) >
          nitems)
        break;

      if (tag != kExtendedTag || payload_length < 2) {
        data_block += payload_length + 1;
        continue;
      }

      unsigned char extended_tag_code = data_block[1];
      if (extended_tag_code != kExtendedVideoCapabilityTag) {
        data_block += payload_length + 1;
        continue;
      }

      // The difference between preferred, IT, and CE video formats
      // doesn't matter. Sets |flag| to true if any of these flags are true.
      if ((data_block[2] & (1 << kPTOverscan)) ||
          (data_block[2] & (1 << kITOverscan)) ||
          (data_block[2] & (1 << kCEOverscan))) {
        *flag = true;
      } else {
        *flag = false;
      }
      return true;
    }
  }

  return false;
}

bool IsInternalOutputName(const std::string& name) {
  return name.find(kInternal_LVDS) == 0 || name.find(kInternal_eDP) == 0 ||
      name.find(kInternal_DSI) == 0;
}

const XRRModeInfo* FindModeInfo(const XRRScreenResources* screen_resources,
                                XID current_mode) {
  for (int m = 0; m < screen_resources->nmode; m++) {
    XRRModeInfo *mode = &screen_resources->modes[m];
    if (mode->id == current_mode)
      return mode;
  }
  return NULL;
}

// Find a mode that matches the given size with highest
// reflesh rate.
RRMode FindOutputModeMatchingSize(
    const XRRScreenResources* screen_resources,
    const XRROutputInfo* output_info,
    size_t width,
    size_t height) {
  RRMode found = None;
  float best_rate = 0;
  bool non_interlaced_found = false;
  for (int i = 0; i < output_info->nmode; ++i) {
    RRMode mode = output_info->modes[i];
    const XRRModeInfo* info = FindModeInfo(screen_resources, mode);

    if (info->width == width && info->height == height) {
      float rate = GetRefreshRate(info);

      if (info->modeFlags & RR_Interlace) {
        if (non_interlaced_found)
          continue;
      } else {
        // Reset the best rate if the non interlaced is
        // found the first time.
        if (!non_interlaced_found) {
          best_rate = rate;
        }
        non_interlaced_found = true;
      }
      if (rate < best_rate)
        continue;

      found = mode;
      best_rate = rate;
    }
  }
  return found;
}

namespace test {

XRRModeInfo CreateModeInfo(int id,
                           int width,
                           int height,
                           bool interlaced,
                           float refresh_rate) {
  XRRModeInfo mode_info = {0};
  mode_info.id = id;
  mode_info.width = width;
  mode_info.height = height;
  if (interlaced)
    mode_info.modeFlags = RR_Interlace;
  if (refresh_rate != 0.0f) {
    mode_info.hTotal = 1;
    mode_info.vTotal = 1;
    mode_info.dotClock = refresh_rate;
  }
  return mode_info;
}

}  // namespace test

}  // namespace chromeos
