// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_
#define CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_
#pragma once

#include <string>

namespace base {
namespace win {
class RegKey;
}
}

namespace installer_util {

// A helper class for parsing and modifying the Omaha additional parameter
// ("ap") client state value for a product.
class ChannelInfo {
 public:

  // Initialize an instance from the "ap" value in a given registry key.
  // Returns false if the value could not be read from the registry.
  bool Initialize(const base::win::RegKey& key);

  // Writes the info to the "ap" value in a given registry key.
  // Returns false if the value could not be written to the registry.
  bool Write(base::win::RegKey* key) const;

  const std::wstring& value() const { return value_; }
  void set_value(const std::wstring& value) { value_ = value; }

  // Determines the update channel for the value.  Possible |channel_name|
  // results are the empty string (stable channel), "beta", and "dev".  Returns
  // false (without modifying |channel_name|) if the channel could not be
  // determined.
  bool GetChannelName(std::wstring* channel_name) const;

  // Returns true if the -CEEE modifier is present in the value.
  bool IsCeee() const;

  // Adds or removes the -CEEE modifier, returning true if the value is
  // modified.
  bool SetCeee(bool value);

  // Returns true if the -full modifier is present in the value.
  bool IsFullInstall() const;

  // Adds or removes the -full modifier, returning true if the value is
  // modified.
  bool SetFullInstall(bool value);

  // Returns true if the -multi modifier is present in the value.
  bool IsMultiInstall() const;

  // Adds or removes the -multi modifier, returning true if the value is
  // modified.
  bool SetMultiInstall(bool value);

 private:
  static bool HasModifier(const wchar_t* modifier,
                          const std::wstring& ap_value);
  static bool SetModifier(const wchar_t* modifier, bool set,
                          std::wstring* ap_value);

  std::wstring value_;
};  // class ChannelInfo

}  // namespace installer_util

#endif  // CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_
