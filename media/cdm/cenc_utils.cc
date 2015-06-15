// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cenc_utils.h"

#include "base/stl_util.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"

namespace media {

// The initialization data for encrypted media files using the ISO Common
// Encryption ('cenc') protection scheme may contain one or more protection
// system specific header ('pssh') boxes.
// ref: https://w3c.github.io/encrypted-media/cenc-format.html

// SystemID for the Common System.
// https://w3c.github.io/encrypted-media/cenc-format.html#common-system
const uint8_t kCommonSystemId[] = { 0x10, 0x77, 0xef, 0xec,
                                    0xc0, 0xb2, 0x4d, 0x02,
                                    0xac, 0xe3, 0x3c, 0x1e,
                                    0x52, 0xe2, 0xfb, 0x4b };

static bool ReadAllPsshBoxes(
    const std::vector<uint8_t>& input,
    std::vector<mp4::FullProtectionSystemSpecificHeader>* pssh_boxes) {
  DCHECK(!input.empty());

  // Verify that |input| contains only 'pssh' boxes. ReadAllChildren() is
  // templated, so it checks that each box in |input| matches the box type of
  // the parameter (in this case mp4::ProtectionSystemSpecificHeader is a
  // 'pssh' box). mp4::ProtectionSystemSpecificHeader doesn't validate the
  // 'pssh' contents, so this simply verifies that |input| only contains
  // 'pssh' boxes and nothing else.
  scoped_ptr<mp4::BoxReader> input_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(
          vector_as_array(&input), input.size()));
  std::vector<mp4::ProtectionSystemSpecificHeader> raw_pssh_boxes;
  if (!input_reader->ReadAllChildren(&raw_pssh_boxes))
    return false;

  // Now that we have |input| parsed into |raw_pssh_boxes|, reparse each one
  // into a mp4::FullProtectionSystemSpecificHeader, which extracts all the
  // relevant fields from the box. Since there may be unparseable 'pssh' boxes
  // (due to unsupported version, for example), this is done one by one,
  // ignoring any boxes that can't be parsed.
  for (const auto& raw_pssh_box : raw_pssh_boxes) {
    scoped_ptr<mp4::BoxReader> raw_pssh_reader(
        mp4::BoxReader::ReadConcatentatedBoxes(
            vector_as_array(&raw_pssh_box.raw_box),
            raw_pssh_box.raw_box.size()));
    // ReadAllChildren() appends any successfully parsed box onto it's
    // parameter, so |pssh_boxes| will contain the collection of successfully
    // parsed 'pssh' boxes. If an error occurs, try the next box.
    if (!raw_pssh_reader->ReadAllChildren(pssh_boxes))
      continue;
  }

  // Must have successfully parsed at least one 'pssh' box.
  return pssh_boxes->size() > 0;
}

bool ValidatePsshInput(const std::vector<uint8_t>& input) {
  // No 'pssh' boxes is considered valid.
  if (input.empty())
    return true;

  std::vector<mp4::FullProtectionSystemSpecificHeader> children;
  return ReadAllPsshBoxes(input, &children);
}

bool GetKeyIdsForCommonSystemId(const std::vector<uint8_t>& input,
                                KeyIdList* key_ids) {
  KeyIdList result;
  std::vector<uint8_t> common_system_id(
      kCommonSystemId, kCommonSystemId + arraysize(kCommonSystemId));

  if (!input.empty()) {
    std::vector<mp4::FullProtectionSystemSpecificHeader> children;
    if (!ReadAllPsshBoxes(input, &children))
      return false;

    // Check all children for an appropriate 'pssh' box, concatenating any
    // key IDs found.
    for (const auto& child : children) {
      if (child.system_id == common_system_id && child.key_ids.size() > 0)
        result.insert(result.end(), child.key_ids.begin(), child.key_ids.end());
    }
  }

  // No matching 'pssh' box found.
  // TODO(jrummell): This should return true only if there was at least one
  // key ID present. However, numerous test files don't contain the 'pssh' box
  // for Common Format, so no keys are found. http://crbug.com/460308
  key_ids->swap(result);
  return true;
}

bool GetPsshData(const std::vector<uint8_t>& input,
                 const std::vector<uint8_t>& system_id,
                 std::vector<uint8_t>* pssh_data) {
  if (input.empty())
    return false;

  std::vector<mp4::FullProtectionSystemSpecificHeader> children;
  if (!ReadAllPsshBoxes(input, &children))
    return false;

  // Check all children for an appropriate 'pssh' box, returning |data| from
  // the first one found.
  for (const auto& child : children) {
    if (child.system_id == system_id) {
      pssh_data->assign(child.data.begin(), child.data.end());
      return true;
    }
  }

  // No matching 'pssh' box found.
  return false;
}

}  // namespace media
