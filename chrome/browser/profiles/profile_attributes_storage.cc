// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_attributes_storage.h"

#include <algorithm>

#include "base/i18n/number_formatting.h"
#include "base/i18n/string_compare.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// First eight are generic icons, which use IDS_NUMBERED_PROFILE_NAME.
const int kDefaultNames[] = {
  IDS_DEFAULT_AVATAR_NAME_8,
  IDS_DEFAULT_AVATAR_NAME_9,
  IDS_DEFAULT_AVATAR_NAME_10,
  IDS_DEFAULT_AVATAR_NAME_11,
  IDS_DEFAULT_AVATAR_NAME_12,
  IDS_DEFAULT_AVATAR_NAME_13,
  IDS_DEFAULT_AVATAR_NAME_14,
  IDS_DEFAULT_AVATAR_NAME_15,
  IDS_DEFAULT_AVATAR_NAME_16,
  IDS_DEFAULT_AVATAR_NAME_17,
  IDS_DEFAULT_AVATAR_NAME_18,
  IDS_DEFAULT_AVATAR_NAME_19,
  IDS_DEFAULT_AVATAR_NAME_20,
  IDS_DEFAULT_AVATAR_NAME_21,
  IDS_DEFAULT_AVATAR_NAME_22,
  IDS_DEFAULT_AVATAR_NAME_23,
  IDS_DEFAULT_AVATAR_NAME_24,
  IDS_DEFAULT_AVATAR_NAME_25,
  IDS_DEFAULT_AVATAR_NAME_26
};

// Compares two ProfileAttributesEntry using locale-sensitive comparison of
// their names. For ties, the profile path is compared next.
class ProfileAttributesSortComparator {
 public:
  explicit ProfileAttributesSortComparator(icu::Collator* collator);
  bool operator()(const ProfileAttributesEntry* const a,
                  const ProfileAttributesEntry* const b) const;
 private:
  icu::Collator* collator_;
};

ProfileAttributesSortComparator::ProfileAttributesSortComparator(
    icu::Collator* collator) : collator_(collator) {}

bool ProfileAttributesSortComparator::operator()(
    const ProfileAttributesEntry* const a,
    const ProfileAttributesEntry* const b) const {
  UCollationResult result = base::i18n::CompareString16WithCollator(
      *collator_, a->GetName(), b->GetName());
  if (result != UCOL_EQUAL)
    return result == UCOL_LESS;

  // If the names are the same, then compare the paths, which must be unique.
  return a->GetPath().value() < b->GetPath().value();
}

// Tries to find an random icon index that satisfies all the given conditions.
// Returns true if an icon was found, false otherwise.
bool GetRandomAvatarIconIndex(
    bool allow_generic_icon,
    bool must_be_unused,
    const std::unordered_set<size_t>& used_icon_indices,
    size_t* out_icon_index) {
  size_t start = allow_generic_icon ? 0 : profiles::GetGenericAvatarIconCount();
  size_t end = profiles::GetDefaultAvatarIconCount();
  size_t count = end - start;

  int rand = base::RandInt(0, count);
  for (size_t i = 0; i < count; ++i) {
    size_t icon_index = start + (rand + i) % count;
    if (!must_be_unused || used_icon_indices.count(icon_index) == 0u) {
      *out_icon_index = icon_index;
      return true;
    }
  }

  return false;
}

}  // namespace

ProfileAttributesStorage::ProfileAttributesStorage(
    PrefService* prefs, const base::FilePath& user_data_dir)
    : prefs_(prefs),
      user_data_dir_(user_data_dir) {
}

ProfileAttributesStorage::~ProfileAttributesStorage() {
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributes() {
  std::vector<ProfileAttributesEntry*> ret;
  for (const auto& path_and_entry : profile_attributes_entries_) {
    ProfileAttributesEntry* entry;
    // Initialize any entries that are not yet initialized.
    bool success = GetProfileAttributesWithPath(
        base::FilePath(path_and_entry.first), &entry);
    DCHECK(success);
    ret.push_back(entry);
  }
  return ret;
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSortedByName() {
  UErrorCode error_code = U_ZERO_ERROR;
  // Use the default collator. The default locale should have been properly
  // set by the time this constructor is called.
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));
  DCHECK(U_SUCCESS(error_code));

  std::vector<ProfileAttributesEntry*> ret = GetAllProfilesAttributes();
  std::sort(ret.begin(), ret.end(),
            ProfileAttributesSortComparator(collator.get()));
  return ret;
}

base::string16 ProfileAttributesStorage::ChooseNameForNewProfile(
    size_t icon_index) const {
  base::string16 name;
  for (int name_index = 1; ; ++name_index) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // Using native digits will break IsDefaultProfileName() below because
    // it uses sscanf.
    // TODO(jshin): fix IsDefaultProfileName to handle native digits.
    name = l10n_util::GetStringFUTF16(IDS_NEW_NUMBERED_PROFILE_NAME,
                                      base::IntToString16(name_index));
#else
    if (icon_index < profiles::GetGenericAvatarIconCount()) {
      name = l10n_util::GetStringFUTF16Int(IDS_NUMBERED_PROFILE_NAME,
                                           name_index);
    } else {
      // TODO(jshin): Check with UX if appending |name_index| to the default
      // name without a space is intended.
      name = l10n_util::GetStringUTF16(
          kDefaultNames[icon_index - profiles::GetGenericAvatarIconCount()]);
      if (name_index > 1)
        name.append(base::FormatNumber(name_index));
    }
#endif

    // Loop through previously named profiles to ensure we're not duplicating.
    std::vector<ProfileAttributesEntry*> entries =
        const_cast<ProfileAttributesStorage*>(this)->GetAllProfilesAttributes();

    if (std::none_of(entries.begin(), entries.end(),
                     [name](ProfileAttributesEntry* entry) {
                       return entry->GetName() == name;
                     })) {
      return name;
    }
  }
}

bool ProfileAttributesStorage::IsDefaultProfileName(
    const base::string16& name) const {
  // Check if it's a "First user" old-style name.
  if (name == l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME) ||
      name == l10n_util::GetStringUTF16(IDS_LEGACY_DEFAULT_PROFILE_NAME))
    return true;

  // Check if it's one of the old-style profile names.
  for (size_t i = 0; i < arraysize(kDefaultNames); ++i) {
    if (name == l10n_util::GetStringUTF16(kDefaultNames[i]))
      return true;
  }

  // Check whether it's one of the "Person %d" style names.
  std::string default_name_format = l10n_util::GetStringFUTF8(
      IDS_NEW_NUMBERED_PROFILE_NAME, base::ASCIIToUTF16("%d"));

  int generic_profile_number;  // Unused. Just a placeholder for sscanf.
  int assignments = sscanf(base::UTF16ToUTF8(name).c_str(),
                           default_name_format.c_str(),
                           &generic_profile_number);
  // Unless it matched the format, this is a custom name.
  return assignments == 1;
}

size_t ProfileAttributesStorage::ChooseAvatarIconIndexForNewProfile() const {
  std::unordered_set<size_t> used_icon_indices;

  std::vector<ProfileAttributesEntry*> entries =
      const_cast<ProfileAttributesStorage*>(this)->GetAllProfilesAttributes();
  for (const ProfileAttributesEntry* entry : entries)
    used_icon_indices.insert(entry->GetAvatarIconIndex());

  size_t icon_index = 0;
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // For ChromeOS and Android, try to find a unique, non-generic icon.
  if (GetRandomAvatarIconIndex(false, true, used_icon_indices, &icon_index))
    return icon_index;
#endif

  // Try to find any unique icon.
  if (GetRandomAvatarIconIndex(true, true, used_icon_indices, &icon_index))
    return icon_index;

  // Settle for any random icon, even if it's not already used.
  if (GetRandomAvatarIconIndex(true, false, used_icon_indices, &icon_index))
    return icon_index;

  NOTREACHED();
  return 0;
}
