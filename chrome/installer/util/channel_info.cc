// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/channel_info.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"

using base::win::RegKey;

namespace {

const wchar_t kChannelBeta[] = L"beta";
const wchar_t kChannelDev[] = L"dev";
const wchar_t kModCeee[] = L"-CEEE";
const wchar_t kModFullInstall[] = L"-full";
const wchar_t kModMultiInstall[] = L"-multi";

const wchar_t* const kChannels[] = {
  kChannelBeta,
  kChannelDev
};

const wchar_t* const kModifiers[] = {
  kModCeee,
  kModFullInstall,
  kModMultiInstall
};

}  // namespace

namespace installer_util {

// static
bool ChannelInfo::HasModifier(const wchar_t* modifier,
                              const std::wstring& ap_value) {
  DCHECK(modifier);
  return ap_value.find(modifier) != std::wstring::npos;
}

// Returns true if |ap_value| is modified.
// static
bool ChannelInfo::SetModifier(const wchar_t* modifier,
                              bool set,
                              std::wstring* ap_value) {
  DCHECK(modifier);
  DCHECK(ap_value);
  std::wstring::size_type position = ap_value->find(modifier);
  if (set) {
    if (position == std::wstring::npos) {
      ap_value->append(modifier);
      return true;
    }
  } else {
    if (position != std::wstring::npos) {
      ap_value->erase(position, std::wstring::traits_type::length(modifier));
      return true;
    }
  }
  return false;
}

bool ChannelInfo::Initialize(const RegKey& key) {
  return key.ReadValue(google_update::kRegApField, &value_);
}

bool ChannelInfo::Write(RegKey* key) const {
  return key->WriteValue(google_update::kRegApField, value_.c_str());
}

bool ChannelInfo::GetChannelName(std::wstring* channel_name) const {
  DCHECK(channel_name);
  if (value_.empty()) {
    channel_name->erase();
    return true;
  } else {
    for (const wchar_t* const* scan = &kChannels[0],
             *const* end = &kChannels[arraysize(kChannels)]; scan != end;
         ++scan) {
      if (value_.find(*scan) != std::wstring::npos) {
        channel_name->assign(*scan);
        return true;
      }
    }
    // There may be modifiers present.  Strip them off and see if we're left
    // with the empty string (stable channel).
    std::wstring tmp_value = value_;
    for (const wchar_t* const* scan = &kModifiers[0],
             *const *end = &kModifiers[arraysize(kModifiers)]; scan != end;
         ++scan) {
      SetModifier(*scan, false, &tmp_value);
    }
    if (tmp_value.empty()) {
      channel_name->erase();
      return true;
    }
  }

  return false;
}

bool ChannelInfo::IsCeee() const {
  return HasModifier(kModCeee, value_);
}

bool ChannelInfo::SetCeee(bool value) {
  return SetModifier(kModCeee, value, &value_);
}

bool ChannelInfo::IsFullInstall() const {
  return HasModifier(kModFullInstall, value_);
}

bool ChannelInfo::SetFullInstall(bool value) {
  return SetModifier(kModFullInstall, value, &value_);
}

bool ChannelInfo::IsMultiInstall() const {
  return HasModifier(kModMultiInstall, value_);
}

bool ChannelInfo::SetMultiInstall(bool value) {
  return SetModifier(kModMultiInstall, value, &value_);
}

}  // namespace installer_util
