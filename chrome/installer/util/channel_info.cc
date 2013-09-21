// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/channel_info.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

using base::win::RegKey;

namespace {

const wchar_t kModChrome[] = L"-chrome";
const wchar_t kModChromeFrame[] = L"-chromeframe";
// TODO(huangs): Remove by M27.
const wchar_t kModAppHostDeprecated[] = L"-apphost";
const wchar_t kModAppLauncher[] = L"-applauncher";
const wchar_t kModMultiInstall[] = L"-multi";
const wchar_t kModReadyMode[] = L"-readymode";
const wchar_t kModStage[] = L"-stage:";
const wchar_t kSfxFull[] = L"-full";
const wchar_t kSfxMigrating[] = L"-migrating";
const wchar_t kSfxMultiFail[] = L"-multifail";

const wchar_t* const kChannels[] = {
  installer::kChromeChannelBeta,
  installer::kChromeChannelDev
};

const wchar_t* const kModifiers[] = {
  kModStage,
  kModMultiInstall,
  kModChrome,
  kModChromeFrame,
  kModAppHostDeprecated,  // TODO(huangs): Remove by M27.
  kModAppLauncher,
  kModReadyMode,
  kSfxMultiFail,
  kSfxMigrating,
  kSfxFull,
};

enum ModifierIndex {
  MOD_STAGE,
  MOD_MULTI_INSTALL,
  MOD_CHROME,
  MOD_CHROME_FRAME,
  MOD_APP_HOST_DEPRECATED,  // TODO(huangs): Remove by M27.
  MOD_APP_LAUNCHER,
  MOD_READY_MODE,
  SFX_MULTI_FAIL,
  SFX_MIGRATING,
  SFX_FULL,
  NUM_MODIFIERS
};

COMPILE_ASSERT(NUM_MODIFIERS == arraysize(kModifiers),
    kModifiers_disagrees_with_ModifierIndex_comma_they_must_match_bang);

// Returns true if the modifier is found, in which case |position| holds the
// location at which the modifier was found.  The number of characters in the
// modifier is returned in |length|, if non-NULL.
bool FindModifier(ModifierIndex index,
                  const std::wstring& ap_value,
                  std::wstring::size_type* position,
                  std::wstring::size_type* length) {
  DCHECK(position != NULL);
  std::wstring::size_type mod_position = std::wstring::npos;
  std::wstring::size_type mod_length =
      std::wstring::traits_type::length(kModifiers[index]);
  const bool mod_takes_arg = (kModifiers[index][mod_length - 1] == L':');
  std::wstring::size_type pos = 0;
  do {
    mod_position = ap_value.find(kModifiers[index], pos, mod_length);
    if (mod_position == std::wstring::npos)
      return false;  // Modifier not found.
    pos = mod_position + mod_length;
    // Modifiers that take an argument gobble up to the next separator or to the
    // end.
    if (mod_takes_arg) {
      pos = ap_value.find(L'-', pos);
      if (pos == std::wstring::npos)
        pos = ap_value.size();
      break;
    }
    // Regular modifiers must be followed by '-' or the end of the string.
  } while (pos != ap_value.size() && ap_value[pos] != L'-');
  DCHECK_NE(mod_position, std::wstring::npos);
  *position = mod_position;
  if (length != NULL)
    *length = pos - mod_position;
  return true;
}

bool HasModifier(ModifierIndex index, const std::wstring& ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  std::wstring::size_type position;
  return FindModifier(index, ap_value, &position, NULL);
}

std::wstring::size_type FindInsertionPoint(ModifierIndex index,
                                           const std::wstring& ap_value) {
  // Return the location of the next modifier.
  std::wstring::size_type result;

  for (int scan = index + 1; scan < NUM_MODIFIERS; ++scan) {
    if (FindModifier(static_cast<ModifierIndex>(scan), ap_value, &result, NULL))
      return result;
  }

  return ap_value.size();
}

// Returns true if |ap_value| is modified.
bool SetModifier(ModifierIndex index, bool set, std::wstring* ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  DCHECK(ap_value);
  std::wstring::size_type position;
  std::wstring::size_type length;
  bool have_modifier = FindModifier(index, *ap_value, &position, &length);
  if (set) {
    if (!have_modifier) {
      ap_value->insert(FindInsertionPoint(index, *ap_value), kModifiers[index]);
      return true;
    }
  } else {
    if (have_modifier) {
      ap_value->erase(position, length);
      return true;
    }
  }
  return false;
}

}  // namespace

namespace installer {

bool ChannelInfo::Initialize(const RegKey& key) {
  LONG result = key.ReadValue(google_update::kRegApField, &value_);
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
         result == ERROR_INVALID_HANDLE;
}

bool ChannelInfo::Write(RegKey* key) const {
  DCHECK(key);
  // Google Update deletes the value when it is empty, so we may as well, too.
  LONG result = value_.empty() ?
      key->DeleteValue(google_update::kRegApField) :
      key->WriteValue(google_update::kRegApField, value_.c_str());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed writing channel info; result: " << result;
    return false;
  }
  return true;
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

bool ChannelInfo::IsAppLauncher() const {
  return HasModifier(MOD_APP_LAUNCHER, value_);
}

bool ChannelInfo::SetAppLauncher(bool value) {
  // Unconditionally remove -apphost since it has been deprecated.
  bool changed_app_host = SetModifier(MOD_APP_HOST_DEPRECATED, false, &value_);
  bool changed_app_launcher = SetModifier(MOD_APP_LAUNCHER, value, &value_);
  return changed_app_host || changed_app_launcher;
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

bool ChannelInfo::SetStage(const wchar_t* stage) {
  std::wstring::size_type position;
  std::wstring::size_type length;
  bool have_modifier = FindModifier(MOD_STAGE, value_, &position, &length);
  if (stage != NULL && *stage != L'\0') {
    std::wstring stage_str(kModStage);
    stage_str.append(stage);
    if (!have_modifier) {
      value_.insert(FindInsertionPoint(MOD_STAGE, value_), stage_str);
      return true;
    }
    if (value_.compare(position, length, stage_str) != 0) {
      value_.replace(position, length, stage_str);
      return true;
    }
  } else {
    if (have_modifier) {
      value_.erase(position, length);
      return true;
    }
  }
  return false;
}

std::wstring ChannelInfo::GetStage() const {
  std::wstring::size_type position;
  std::wstring::size_type length;

  if (FindModifier(MOD_STAGE, value_, &position, &length)) {
    // Return the portion after the prefix.
    std::wstring::size_type pfx_length =
        std::wstring::traits_type::length(kModStage);
    DCHECK_LE(pfx_length, length);
    return value_.substr(position + pfx_length, length - pfx_length);
  }
  return std::wstring();
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

bool ChannelInfo::SetMigratingSuffix(bool value) {
  return SetModifier(SFX_MIGRATING, value, &value_);
}

bool ChannelInfo::HasMigratingSuffix() const {
  return HasModifier(SFX_MIGRATING, value_);
}

bool ChannelInfo::RemoveAllModifiersAndSuffixes() {
  bool modified = false;

  for (int scan = 0; scan < NUM_MODIFIERS; ++scan) {
    ModifierIndex index = static_cast<ModifierIndex>(scan);
    modified = SetModifier(index, false, &value_) || modified;
  }

  return modified;
}

}  // namespace installer
