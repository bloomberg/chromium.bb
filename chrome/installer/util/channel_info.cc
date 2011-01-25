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
const wchar_t kModChrome[] = L"-chrome";
const wchar_t kModChromeFrame[] = L"-chromeframe";
const wchar_t kModMultiInstall[] = L"-multi";
const wchar_t kModReadyMode[] = L"-readymode";
const wchar_t kSfxFull[] = L"-full";
const wchar_t kSfxMultiFail[] = L"-multifail";

const wchar_t* const kChannels[] = {
  kChannelBeta,
  kChannelDev
};

const wchar_t* const kModifiers[] = {
  kModMultiInstall,
  kModChrome,
  kModChromeFrame,
  kModCeee,
  kModReadyMode,
  kSfxMultiFail,
  kSfxFull
};

enum ModifierIndex {
  MOD_MULTI_INSTALL,
  MOD_CHROME,
  MOD_CHROME_FRAME,
  MOD_CEEE,
  MOD_READY_MODE,
  SFX_MULTI_FAIL,
  SFX_FULL,
  NUM_MODIFIERS
};

COMPILE_ASSERT(NUM_MODIFIERS == arraysize(kModifiers),
    kModifiers_disagrees_with_ModifierIndex_comma_they_must_match_bang);

// Returns true if the modifier is found, in which case |position| holds the
// location at which the modifier was found.
bool FindModifier(ModifierIndex index,
                  const std::wstring& ap_value,
                  std::wstring::size_type* position) {
  DCHECK(position != NULL);
  std::wstring::size_type mod_length =
      std::wstring::traits_type::length(kModifiers[index]);
  for (std::wstring::size_type pos = 0; ; ) {
    *position = ap_value.find(kModifiers[index], pos, mod_length);
    if (*position == std::wstring::npos)
      break;
    // The modifier must be either at the end of the string or followed by -.
    pos = *position + mod_length;
    if (pos == ap_value.size() || ap_value[pos] == L'-')
      return true;
  }
  return false;
}

bool HasModifier(ModifierIndex index, const std::wstring& ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  std::wstring::size_type position;
  return FindModifier(index, ap_value, &position);
}

std::wstring::size_type FindInsertionPoint(ModifierIndex index,
                                           const std::wstring& ap_value) {
  // Return the location of the next modifier.
  std::wstring::size_type result;

  for (int scan = index + 1; scan < NUM_MODIFIERS; ++scan) {
    if (FindModifier(static_cast<ModifierIndex>(scan), ap_value, &result))
      return result;
  }

  return ap_value.size();
}

// Returns true if |ap_value| is modified.
bool SetModifier(ModifierIndex index, bool set, std::wstring* ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  DCHECK(ap_value);
  std::wstring::size_type position;
  bool have_modifier = FindModifier(index, *ap_value, &position);
  if (set) {
    if (!have_modifier) {
      ap_value->insert(FindInsertionPoint(index, *ap_value), kModifiers[index]);
      return true;
    }
  } else {
    if (have_modifier) {
      ap_value->erase(position,
                      std::wstring::traits_type::length(kModifiers[index]));
      return true;
    }
  }
  return false;
}

}  // namespace

namespace installer {

bool ChannelInfo::Initialize(const RegKey& key) {
  return (key.ReadValue(google_update::kRegApField, &value_) == ERROR_SUCCESS);
}

bool ChannelInfo::Write(RegKey* key) const {
  return (key->WriteValue(google_update::kRegApField, value_.c_str()) ==
      ERROR_SUCCESS);
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
    for (int i = 0; i != NUM_MODIFIERS; ++i) {
      SetModifier(static_cast<ModifierIndex>(i), false, &tmp_value);
    }
    if (tmp_value.empty()) {
      channel_name->erase();
      return true;
    }
  }

  return false;
}

bool ChannelInfo::EqualsBaseOf(const ChannelInfo& other) const {
  std::wstring::size_type this_base_end;
  std::wstring::size_type other_base_end;

  if (!FindModifier(MOD_MULTI_INSTALL, value_, &this_base_end))
    this_base_end = FindInsertionPoint(MOD_MULTI_INSTALL, value_);
  if (!FindModifier(MOD_MULTI_INSTALL, other.value_, &other_base_end))
    other_base_end = FindInsertionPoint(MOD_MULTI_INSTALL, other.value_);
  return value_.compare(0, this_base_end,
                        other.value_.c_str(), other_base_end) == 0;
}

bool ChannelInfo::IsCeee() const {
  return HasModifier(MOD_CEEE, value_);
}

bool ChannelInfo::SetCeee(bool value) {
  return SetModifier(MOD_CEEE, value, &value_);
}

bool ChannelInfo::IsChrome() const {
  return HasModifier(MOD_CHROME, value_);
}

bool ChannelInfo::SetChrome(bool value) {
  return SetModifier(MOD_CHROME, value, &value_);
}

bool ChannelInfo::IsChromeFrame() const {
  return HasModifier(MOD_CHROME_FRAME, value_);
}

bool ChannelInfo::SetChromeFrame(bool value) {
  return SetModifier(MOD_CHROME_FRAME, value, &value_);
}

bool ChannelInfo::IsMultiInstall() const {
  return HasModifier(MOD_MULTI_INSTALL, value_);
}

bool ChannelInfo::SetMultiInstall(bool value) {
  return SetModifier(MOD_MULTI_INSTALL, value, &value_);
}

bool ChannelInfo::IsReadyMode() const {
  return HasModifier(MOD_READY_MODE, value_);
}

bool ChannelInfo::SetReadyMode(bool value) {
  return SetModifier(MOD_READY_MODE, value, &value_);
}

bool ChannelInfo::HasFullSuffix() const {
  return HasModifier(SFX_FULL, value_);
}

bool ChannelInfo::SetFullSuffix(bool value) {
  return SetModifier(SFX_FULL, value, &value_);
}

bool ChannelInfo::HasMultiFailSuffix() const {
  return HasModifier(SFX_MULTI_FAIL, value_);
}

bool ChannelInfo::SetMultiFailSuffix(bool value) {
  return SetModifier(SFX_MULTI_FAIL, value, &value_);
}

}  // namespace installer
