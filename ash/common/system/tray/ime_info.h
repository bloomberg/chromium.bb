// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_IME_INFO_H_
#define ASH_COMMON_SYSTEM_TRAY_IME_INFO_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace ash {

struct ASH_EXPORT IMEInfo {
  IMEInfo();
  IMEInfo(const IMEInfo& other);
  ~IMEInfo();

  // True if the IME the current IME.
  bool selected;

  // True if the IME is a third-party extension.
  bool third_party;

  // ID that identifies the IME (e.g., "t:latn-post", "pinyin", "hangul").
  std::string id;

  // Long name of the IME, which is used to specify the user-visible name.
  base::string16 name;

  // Medium name of the IME, which is same with short name in most cases, unless
  // find in a table for medium length names.
  base::string16 medium_name;

  // Indicator of the IME (e.g., "US"). If indicator is empty, use the first two
  // character in its preferred keyboard layout or language code (e.g., "ko",
  // "ja", "en-US").
  base::string16 short_name;
};

using IMEInfoList = std::vector<IMEInfo>;

struct ASH_EXPORT IMEPropertyInfo {
  IMEPropertyInfo();
  ~IMEPropertyInfo();

  // True if the property is a selection item.
  bool selected;

  // The key which identifies the property, e.g. "InputMode.HalfWidthKatakana".
  std::string key;

  // The description of the property, e.g. "Switch to full punctuation mode",
  // "Hiragana".
  base::string16 name;
};

using IMEPropertyInfoList = std::vector<IMEPropertyInfo>;

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_IME_INFO_H_
