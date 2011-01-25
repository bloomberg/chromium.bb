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

namespace installer {

// A helper class for parsing and modifying the Google Update additional
// parameter ("ap") client state value for a product.
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
  bool Equals(const ChannelInfo& other) const {
    return value_ == other.value_;
  }

  // Determines the update channel for the value.  Possible |channel_name|
  // results are the empty string (stable channel), "beta", and "dev".  Returns
  // false (without modifying |channel_name|) if the channel could not be
  // determined.
  bool GetChannelName(std::wstring* channel_name) const;

  // Returns true if this object and |other| have a common base (that which
  // remains when all modifiers and suffixes are omitted).
  bool EqualsBaseOf(const ChannelInfo& other) const;

  // Returns true if the -CEEE modifier is present in the value.
  bool IsCeee() const;

  // Adds or removes the -CEEE modifier, returning true if the value is
  // modified.
  bool SetCeee(bool value);

  // Returns true if the -chrome modifier is present in the value.
  bool IsChrome() const;

  // Adds or removes the -chrome modifier, returning true if the value is
  // modified.
  bool SetChrome(bool value);

  // Returns true if the -chromeframe modifier is present in the value.
  bool IsChromeFrame() const;

  // Adds or removes the -chromeframe modifier, returning true if the value is
  // modified.
  bool SetChromeFrame(bool value);

  // Returns true if the -multi modifier is present in the value.
  bool IsMultiInstall() const;

  // Adds or removes the -multi modifier, returning true if the value is
  // modified.
  bool SetMultiInstall(bool value);

  // Returns true if the -readymode modifier is present in the value.
  bool IsReadyMode() const;

  // Adds or removes the -readymode modifier, returning true if the value is
  // modified.
  bool SetReadyMode(bool value);

  // Returns true if the -full suffix is present in the value.
  bool HasFullSuffix() const;

  // Adds or removes the -full suffix, returning true if the value is
  // modified.
  bool SetFullSuffix(bool value);

  // Returns true if the -multifail suffix is present in the value.
  bool HasMultiFailSuffix() const;

  // Adds or removes the -multifail suffix, returning true if the value is
  // modified.
  bool SetMultiFailSuffix(bool value);

 private:
  std::wstring value_;
};  // class ChannelInfo

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_
