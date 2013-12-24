// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/character_encoding.h"

#include <map>
#include <set>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

using content::BrowserThread;

namespace {

// The maximum length of short list of recently user selected encodings is 3.
const size_t kUserSelectedEncodingsMaxLength = 3;

typedef struct {
  int resource_id;
  const char* name;
  int category_string_id;
} CanonicalEncodingData;

// An array of all supported canonical encoding names.
const CanonicalEncodingData kCanonicalEncodingNames[] = {
  { IDC_ENCODING_UTF8, "UTF-8", IDS_ENCODING_UNICODE },
  { IDC_ENCODING_UTF16LE, "UTF-16LE", IDS_ENCODING_UNICODE },
  { IDC_ENCODING_ISO88591, "ISO-8859-1", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_WINDOWS1252, "windows-1252", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_GBK, "GBK", IDS_ENCODING_SIMP_CHINESE },
  { IDC_ENCODING_GB18030, "gb18030", IDS_ENCODING_SIMP_CHINESE },
  { IDC_ENCODING_BIG5, "Big5", IDS_ENCODING_TRAD_CHINESE },
  { IDC_ENCODING_BIG5HKSCS, "Big5-HKSCS", IDS_ENCODING_TRAD_CHINESE },
  { IDC_ENCODING_KOREAN, "EUC-KR", IDS_ENCODING_KOREAN },
  { IDC_ENCODING_SHIFTJIS, "Shift_JIS", IDS_ENCODING_JAPANESE },
  { IDC_ENCODING_EUCJP, "EUC-JP", IDS_ENCODING_JAPANESE },
  { IDC_ENCODING_ISO2022JP, "ISO-2022-JP", IDS_ENCODING_JAPANESE },
  { IDC_ENCODING_THAI, "windows-874", IDS_ENCODING_THAI },
  { IDC_ENCODING_ISO885915, "ISO-8859-15", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_MACINTOSH, "macintosh", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_ISO88592, "ISO-8859-2", IDS_ENCODING_CENTRAL_EUROPEAN },
  { IDC_ENCODING_WINDOWS1250, "windows-1250", IDS_ENCODING_CENTRAL_EUROPEAN },
  { IDC_ENCODING_ISO88595, "ISO-8859-5", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_WINDOWS1251, "windows-1251", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_KOI8R, "KOI8-R", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_KOI8U, "KOI8-U", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_ISO88597, "ISO-8859-7", IDS_ENCODING_GREEK },
  { IDC_ENCODING_WINDOWS1253, "windows-1253", IDS_ENCODING_GREEK },
  { IDC_ENCODING_WINDOWS1254, "windows-1254", IDS_ENCODING_TURKISH },
  { IDC_ENCODING_WINDOWS1256, "windows-1256", IDS_ENCODING_ARABIC },
  { IDC_ENCODING_ISO88596, "ISO-8859-6", IDS_ENCODING_ARABIC },
  { IDC_ENCODING_WINDOWS1255, "windows-1255", IDS_ENCODING_HEBREW },
  { IDC_ENCODING_ISO88598I, "ISO-8859-8-I", IDS_ENCODING_HEBREW },
  { IDC_ENCODING_ISO88598, "ISO-8859-8", IDS_ENCODING_HEBREW },
  { IDC_ENCODING_WINDOWS1258, "windows-1258", IDS_ENCODING_VIETNAMESE },
  { IDC_ENCODING_ISO88594, "ISO-8859-4", IDS_ENCODING_BALTIC },
  { IDC_ENCODING_ISO885913, "ISO-8859-13", IDS_ENCODING_BALTIC },
  { IDC_ENCODING_WINDOWS1257, "windows-1257", IDS_ENCODING_BALTIC },
  { IDC_ENCODING_ISO88593, "ISO-8859-3", IDS_ENCODING_SOUTH_EUROPEAN },
  { IDC_ENCODING_ISO885910, "ISO-8859-10", IDS_ENCODING_NORDIC },
  { IDC_ENCODING_ISO885914, "ISO-8859-14", IDS_ENCODING_CELTIC },
  { IDC_ENCODING_ISO885916, "ISO-8859-16", IDS_ENCODING_ROMANIAN },
};

const int kCanonicalEncodingNamesLength = arraysize(kCanonicalEncodingNames);

typedef std::map<int, std::pair<const char*, int> >
    IdToCanonicalEncodingNameMapType;
typedef std::map<const std::string, int> CanonicalEncodingNameToIdMapType;

typedef struct {
  const char* canonical_form;
  const char* display_form;
} CanonicalEncodingDisplayNamePair;

const CanonicalEncodingDisplayNamePair kCanonicalDisplayNameOverrides[] = {
  // Only lists the canonical names where we want a different form for display.
  { "macintosh", "Macintosh" },
  { "windows-874", "Windows-874" },
  { "windows-1250", "Windows-1250" },
  { "windows-1251", "Windows-1251" },
  { "windows-1252", "Windows-1252" },
  { "windows-1253", "Windows-1253" },
  { "windows-1254", "Windows-1254" },
  { "windows-1255", "Windows-1255" },
  { "windows-1256", "Windows-1256" },
  { "windows-1257", "Windows-1257" },
  { "windows-1258", "Windows-1258" },
};

const int kCanonicalDisplayNameOverridesLength =
    arraysize(kCanonicalDisplayNameOverrides);

typedef std::map<std::string, const char*> CanonicalNameDisplayNameMapType;

class CanonicalEncodingMap {
 public:
  CanonicalEncodingMap() {}
  const IdToCanonicalEncodingNameMapType* GetIdToCanonicalEncodingNameMapData();
  const CanonicalEncodingNameToIdMapType* GetCanonicalEncodingNameToIdMapData();
  const CanonicalNameDisplayNameMapType* GetCanonicalNameDisplayNameMapData();
  std::vector<int>* locale_dependent_encoding_ids() {
    return &locale_dependent_encoding_ids_;
  }

  std::vector<CharacterEncoding::EncodingInfo>* current_display_encodings() {
    return &current_display_encodings_;
  }

 private:
  scoped_ptr<IdToCanonicalEncodingNameMapType> id_to_encoding_name_map_;
  scoped_ptr<CanonicalEncodingNameToIdMapType> encoding_name_to_id_map_;
  scoped_ptr<CanonicalNameDisplayNameMapType>
      encoding_name_to_display_name_map_;
  std::vector<int> locale_dependent_encoding_ids_;
  std::vector<CharacterEncoding::EncodingInfo> current_display_encodings_;

  DISALLOW_COPY_AND_ASSIGN(CanonicalEncodingMap);
};

const IdToCanonicalEncodingNameMapType*
    CanonicalEncodingMap::GetIdToCanonicalEncodingNameMapData() {
  // Testing and building map is not thread safe, this function is supposed to
  // only run in UI thread. Myabe I should add a lock in here for making it as
  // thread safe.
  if (!id_to_encoding_name_map_.get()) {
    id_to_encoding_name_map_.reset(new IdToCanonicalEncodingNameMapType);
    for (int i = 0; i < kCanonicalEncodingNamesLength; ++i) {
      int resource_id = kCanonicalEncodingNames[i].resource_id;
      (*id_to_encoding_name_map_)[resource_id] =
        std::make_pair(kCanonicalEncodingNames[i].name,
                       kCanonicalEncodingNames[i].category_string_id);
    }
  }
  return id_to_encoding_name_map_.get();
}

const CanonicalEncodingNameToIdMapType*
    CanonicalEncodingMap::GetCanonicalEncodingNameToIdMapData() {
  if (!encoding_name_to_id_map_.get()) {
    encoding_name_to_id_map_.reset(new CanonicalEncodingNameToIdMapType);
    for (int i = 0; i < kCanonicalEncodingNamesLength; ++i) {
      (*encoding_name_to_id_map_)[kCanonicalEncodingNames[i].name] =
          kCanonicalEncodingNames[i].resource_id;
    }
  }
  return encoding_name_to_id_map_.get();
}

const CanonicalNameDisplayNameMapType*
    CanonicalEncodingMap::GetCanonicalNameDisplayNameMapData() {
  if (!encoding_name_to_display_name_map_.get()) {
    encoding_name_to_display_name_map_.reset(
        new CanonicalNameDisplayNameMapType);
    // First store the names in the kCanonicalEncodingNames list.
    for (int i = 0; i < kCanonicalEncodingNamesLength; ++i) {
      (*encoding_name_to_display_name_map_)[kCanonicalEncodingNames[i].name] =
          kCanonicalEncodingNames[i].name;
    }
    // Then save in the overrides.
    for (int i = 0; i < kCanonicalDisplayNameOverridesLength; ++i) {
      (*encoding_name_to_display_name_map_)
          [kCanonicalDisplayNameOverrides[i].canonical_form] =
          kCanonicalDisplayNameOverrides[i].display_form;
    }
    DCHECK(static_cast<int>(encoding_name_to_display_name_map_->size()) ==
           kCanonicalEncodingNamesLength)
        << "Got an override that wasn't in the encoding list";
  }
  return encoding_name_to_display_name_map_.get();
}

// A static map object which contains all resourceid-nonsequenced canonical
// encoding names.
CanonicalEncodingMap* CanonicalEncodingMapSingleton() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static CanonicalEncodingMap* singleton = new CanonicalEncodingMap;
  return singleton;
}

const int kDefaultEncodingMenus[] = {
  IDC_ENCODING_UTF16LE,
  IDC_ENCODING_ISO88591,
  IDC_ENCODING_WINDOWS1252,
  IDC_ENCODING_GBK,
  IDC_ENCODING_GB18030,
  IDC_ENCODING_BIG5,
  IDC_ENCODING_BIG5HKSCS,
  IDC_ENCODING_KOREAN,
  IDC_ENCODING_SHIFTJIS,
  IDC_ENCODING_EUCJP,
  IDC_ENCODING_ISO2022JP,
  IDC_ENCODING_THAI,
  IDC_ENCODING_ISO885915,
  IDC_ENCODING_MACINTOSH,
  IDC_ENCODING_ISO88592,
  IDC_ENCODING_WINDOWS1250,
  IDC_ENCODING_ISO88595,
  IDC_ENCODING_WINDOWS1251,
  IDC_ENCODING_KOI8R,
  IDC_ENCODING_KOI8U,
  IDC_ENCODING_ISO88597,
  IDC_ENCODING_WINDOWS1253,
  IDC_ENCODING_WINDOWS1254,
  IDC_ENCODING_WINDOWS1256,
  IDC_ENCODING_ISO88596,
  IDC_ENCODING_WINDOWS1255,
  IDC_ENCODING_ISO88598I,
  IDC_ENCODING_ISO88598,
  IDC_ENCODING_WINDOWS1258,
  IDC_ENCODING_ISO88594,
  IDC_ENCODING_ISO885913,
  IDC_ENCODING_WINDOWS1257,
  IDC_ENCODING_ISO88593,
  IDC_ENCODING_ISO885910,
  IDC_ENCODING_ISO885914,
  IDC_ENCODING_ISO885916,
};

const int kDefaultEncodingMenusLength = arraysize(kDefaultEncodingMenus);

// Parse the input |encoding_list| which is a encoding list separated with
// comma, get available encoding ids and save them to |available_list|.
// The parameter |maximum_size| indicates maximum size of encoding items we
// want to get from the |encoding_list|.
void ParseEncodingListSeparatedWithComma(
    const std::string& encoding_list, std::vector<int>* const available_list,
    size_t maximum_size) {
  base::StringTokenizer tokenizer(encoding_list, ",");
  while (tokenizer.GetNext()) {
    int id = CharacterEncoding::GetCommandIdByCanonicalEncodingName(
        tokenizer.token());
    // Ignore invalid encoding.
    if (!id)
      continue;
    available_list->push_back(id);
    if (available_list->size() == maximum_size)
      return;
  }
}

base::string16 GetEncodingDisplayName(const std::string& encoding_name,
                                      int category_string_id) {
  base::string16 category_name = l10n_util::GetStringUTF16(category_string_id);
  if (category_string_id != IDS_ENCODING_KOREAN &&
      category_string_id != IDS_ENCODING_THAI &&
      category_string_id != IDS_ENCODING_TURKISH) {
    const CanonicalNameDisplayNameMapType* map =
        CanonicalEncodingMapSingleton()->GetCanonicalNameDisplayNameMapData();
    DCHECK(map);

    CanonicalNameDisplayNameMapType::const_iterator found_name =
        map->find(encoding_name);
    DCHECK(found_name != map->end());
    return l10n_util::GetStringFUTF16(IDS_ENCODING_DISPLAY_TEMPLATE,
                                      category_name,
                                      base::ASCIIToUTF16(found_name->second));
  }
  return category_name;
}

int GetEncodingCategoryStringIdByCommandId(int id) {
  const IdToCanonicalEncodingNameMapType* map =
      CanonicalEncodingMapSingleton()->GetIdToCanonicalEncodingNameMapData();
  DCHECK(map);

  IdToCanonicalEncodingNameMapType::const_iterator found_name = map->find(id);
  if (found_name != map->end())
    return found_name->second.second;
  return 0;
}

std::string GetEncodingCategoryStringByCommandId(int id) {
  int category_id = GetEncodingCategoryStringIdByCommandId(id);
  if (category_id)
    return l10n_util::GetStringUTF8(category_id);
  return std::string();
}

}  // namespace

CharacterEncoding::EncodingInfo::EncodingInfo(int id)
    : encoding_id(id) {
  encoding_category_name =
      base::UTF8ToUTF16(GetEncodingCategoryStringByCommandId(id));
  encoding_display_name = GetCanonicalEncodingDisplayNameByCommandId(id);
}

// Static.
int CharacterEncoding::GetCommandIdByCanonicalEncodingName(
    const std::string& encoding_name) {
  const CanonicalEncodingNameToIdMapType* map =
      CanonicalEncodingMapSingleton()->GetCanonicalEncodingNameToIdMapData();
  DCHECK(map);

  CanonicalEncodingNameToIdMapType::const_iterator found_id =
      map->find(encoding_name);
  if (found_id != map->end())
    return found_id->second;
  return 0;
}

// Static.
std::string CharacterEncoding::GetCanonicalEncodingNameByCommandId(int id) {
  const IdToCanonicalEncodingNameMapType* map =
      CanonicalEncodingMapSingleton()->GetIdToCanonicalEncodingNameMapData();
  DCHECK(map);

  IdToCanonicalEncodingNameMapType::const_iterator found_name = map->find(id);
  if (found_name != map->end())
    return found_name->second.first;
  return std::string();
}

// Static.
base::string16 CharacterEncoding::GetCanonicalEncodingDisplayNameByCommandId(
    int id) {
  const IdToCanonicalEncodingNameMapType* map =
      CanonicalEncodingMapSingleton()->GetIdToCanonicalEncodingNameMapData();
  DCHECK(map);

  IdToCanonicalEncodingNameMapType::const_iterator found_name = map->find(id);
  if (found_name != map->end())
    return GetEncodingDisplayName(found_name->second.first,
                                  found_name->second.second);
  return base::string16();
}

// Static.
// Return count number of all supported canonical encoding.
int CharacterEncoding::GetSupportCanonicalEncodingCount() {
  return kCanonicalEncodingNamesLength;
}

// Static.
std::string CharacterEncoding::GetCanonicalEncodingNameByIndex(int index) {
  if (index < kCanonicalEncodingNamesLength)
    return kCanonicalEncodingNames[index].name;
  return std::string();
}

// Static.
base::string16 CharacterEncoding::GetCanonicalEncodingDisplayNameByIndex(
    int index) {
  if (index < kCanonicalEncodingNamesLength)
    return GetEncodingDisplayName(kCanonicalEncodingNames[index].name,
        kCanonicalEncodingNames[index].category_string_id);
  return base::string16();
}

// Static.
int CharacterEncoding::GetEncodingCommandIdByIndex(int index) {
  if (index < kCanonicalEncodingNamesLength)
    return kCanonicalEncodingNames[index].resource_id;
  return 0;
}

// Static.
std::string CharacterEncoding::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  // If the input alias_name is already canonical encoding name, just return it.
  const CanonicalEncodingNameToIdMapType* map =
      CanonicalEncodingMapSingleton()->GetCanonicalEncodingNameToIdMapData();
  DCHECK(map);

  CanonicalEncodingNameToIdMapType::const_iterator found_id =
      map->find(alias_name);
  if (found_id != map->end())
    return alias_name;

  UErrorCode error_code = U_ZERO_ERROR;

  const char* canonical_name = ucnv_getCanonicalName(
      alias_name.c_str(), "MIME", &error_code);
  // If failed,  then try IANA next.
  if (U_FAILURE(error_code) || !canonical_name) {
    error_code = U_ZERO_ERROR;
    canonical_name = ucnv_getCanonicalName(
      alias_name.c_str(), "IANA", &error_code);
  }

  if (canonical_name) {
    // TODO(jnd) use a map to handle all customized {alias, canonical}
    // encoding mappings if we have more than one pair.
    // We don't want to add an unnecessary charset to the encoding menu, so we
    // alias 'US-ASCII' to 'ISO-8859-1' in our UI without touching WebKit.
    // http://crbug.com/15801.
    if (alias_name == "US-ASCII")
      return GetCanonicalEncodingNameByCommandId(IDC_ENCODING_ISO88591);
    return canonical_name;
  } else {
    return std::string();
  }
}

// Static
// According to the behavior of user recently selected encoding short list in
// Firefox, we always put UTF-8 as top position, after then put user
// recent selected encodings, then put local dependent encoding items.
// At last, we put all remaining encoding items.
const std::vector<CharacterEncoding::EncodingInfo>*
    CharacterEncoding::GetCurrentDisplayEncodings(
    const std::string& locale,
    const std::string& locale_encodings,
    const std::string& recently_select_encodings) {
  std::vector<int>* const locale_dependent_encoding_list =
      CanonicalEncodingMapSingleton()->locale_dependent_encoding_ids();
  std::vector<CharacterEncoding::EncodingInfo>* const encoding_list =
      CanonicalEncodingMapSingleton()->current_display_encodings();

  // Initialize locale dependent static encoding list.
  if (locale_dependent_encoding_list->empty() && !locale_encodings.empty())
    ParseEncodingListSeparatedWithComma(locale_encodings,
                                        locale_dependent_encoding_list,
                                        kUserSelectedEncodingsMaxLength);

  CR_DEFINE_STATIC_LOCAL(std::string, cached_user_selected_encodings, ());
  // Build current display encoding list.
  if (encoding_list->empty() ||
      cached_user_selected_encodings != recently_select_encodings) {
    // Update user recently selected encodings.
    cached_user_selected_encodings = recently_select_encodings;
    // Clear old encoding list since user recently selected encodings changed.
    encoding_list->clear();
    // Always add UTF-8 to first encoding position.
    encoding_list->push_back(EncodingInfo(IDC_ENCODING_UTF8));
    std::set<int> inserted_encoding;
    inserted_encoding.insert(IDC_ENCODING_UTF8);

    // Parse user recently selected encodings and get list
    std::vector<int> recently_select_encoding_list;
    ParseEncodingListSeparatedWithComma(recently_select_encodings,
                                        &recently_select_encoding_list,
                                        kUserSelectedEncodingsMaxLength);

    // Put 'cached encodings' (dynamic encoding list) after 'local dependent
    // encoding list'.
    recently_select_encoding_list.insert(recently_select_encoding_list.begin(),
        locale_dependent_encoding_list->begin(),
        locale_dependent_encoding_list->end());
    for (std::vector<int>::iterator it = recently_select_encoding_list.begin();
         it != recently_select_encoding_list.end(); ++it) {
        // Test whether we have met this encoding id.
        bool ok = inserted_encoding.insert(*it).second;
        // Duplicated encoding, ignore it. Ideally, this situation should not
        // happened, but just in case some one manually edit preference file.
        if (!ok)
          continue;
        encoding_list->push_back(EncodingInfo(*it));
    }
    // Append a separator;
    encoding_list->push_back(EncodingInfo(0));

    // We need to keep "Unicode (UTF-16LE)" always at the top (among the rest
    // of encodings) instead of being sorted along with other encodings. So if
    // "Unicode (UTF-16LE)" is already in previous encodings, sort the rest
    // of encodings. Otherwise Put "Unicode (UTF-16LE)" on the first of the
    // rest of encodings, skip "Unicode (UTF-16LE)" and sort all left encodings.
    int start_sorted_index = encoding_list->size();
    if (inserted_encoding.find(IDC_ENCODING_UTF16LE) ==
        inserted_encoding.end()) {
      encoding_list->push_back(EncodingInfo(IDC_ENCODING_UTF16LE));
      inserted_encoding.insert(IDC_ENCODING_UTF16LE);
      start_sorted_index++;
    }

    // Add the rest of encodings that are neither in the static encoding list
    // nor in the list of recently selected encodings.
    // Build the encoding list sorted in the current locale sorting order.
    for (int i = 0; i < kDefaultEncodingMenusLength; ++i) {
      int id = kDefaultEncodingMenus[i];
      // We have inserted this encoding, skip it.
      if (inserted_encoding.find(id) != inserted_encoding.end())
        continue;
      encoding_list->push_back(EncodingInfo(id));
    }
    // Sort the encoding list.
    l10n_util::SortVectorWithStringKey(locale,
                                       encoding_list,
                                       start_sorted_index,
                                       encoding_list->size(),
                                       true);
  }
  DCHECK(!encoding_list->empty());
  return encoding_list;
}

// Static
bool CharacterEncoding::UpdateRecentlySelectedEncoding(
    const std::string& original_selected_encodings,
    int new_selected_encoding_id,
    std::string* selected_encodings) {
  // Get encoding name.
  std::string encoding_name =
      GetCanonicalEncodingNameByCommandId(new_selected_encoding_id);
  DCHECK(!encoding_name.empty());
  // Check whether the new encoding is in local dependent encodings or original
  // recently selected encodings. If yes, do not add it.
  std::vector<int>* locale_dependent_encoding_list =
      CanonicalEncodingMapSingleton()->locale_dependent_encoding_ids();
  DCHECK(locale_dependent_encoding_list);
  std::vector<int> selected_encoding_list;
  ParseEncodingListSeparatedWithComma(original_selected_encodings,
                                      &selected_encoding_list,
                                      kUserSelectedEncodingsMaxLength);
  // Put 'cached encodings' (dynamic encoding list) after 'local dependent
  // encoding list' for check.
  std::vector<int> top_encoding_list(*locale_dependent_encoding_list);
  // UTF8 is always in our optimized encoding list.
  top_encoding_list.insert(top_encoding_list.begin(), IDC_ENCODING_UTF8);
  top_encoding_list.insert(top_encoding_list.end(),
                           selected_encoding_list.begin(),
                           selected_encoding_list.end());
  for (std::vector<int>::const_iterator it = top_encoding_list.begin();
       it != top_encoding_list.end(); ++it)
    if (*it == new_selected_encoding_id)
      return false;
  // Need to add the encoding id to recently selected encoding list.
  // Remove the last encoding in original list.
  if (selected_encoding_list.size() == kUserSelectedEncodingsMaxLength)
    selected_encoding_list.pop_back();
  // Insert new encoding to head of selected encoding list.
  *selected_encodings = encoding_name;
  // Generate the string for rest selected encoding list.
  for (std::vector<int>::const_iterator it = selected_encoding_list.begin();
       it != selected_encoding_list.end(); ++it) {
    selected_encodings->append(1, L',');
    selected_encodings->append(GetCanonicalEncodingNameByCommandId(*it));
  }
  return true;
}
