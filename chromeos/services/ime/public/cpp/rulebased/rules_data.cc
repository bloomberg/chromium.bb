// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ar.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ckb_ar.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ckb_en.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/deva_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/fa.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/km.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/lo.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ne_inscript.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ru_phone_aatseel.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ru_phone_yazhert.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_inscript.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_typewriter.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th_pattajoti.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th_tis.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/us.h"
#include "third_party/re2/src/re2/re2.h"

namespace chromeos {
namespace ime {
namespace rulebased {

namespace {

struct RawDataEntry {
  const wchar_t** key_map;
  const uint8_t key_map_count;
  const uint8_t* key_map_index;
  bool is_102_keyboard;
  const char** transforms;
  const uint16_t transforms_count;
  const char* history_prune;

  RawDataEntry(const wchar_t** map,
               uint8_t map_count,
               const uint8_t* map_index,
               bool is_102)
      : RawDataEntry(map, map_count, map_index, is_102, nullptr, 0, nullptr) {}

  RawDataEntry(const wchar_t** map,
               uint8_t map_count,
               const uint8_t* map_index,
               bool is_102,
               const char** trans,
               uint16_t trans_count,
               const char* prune)
      : key_map(map),
        key_map_count(map_count),
        key_map_index(map_index),
        is_102_keyboard(is_102),
        transforms(trans),
        transforms_count(trans_count),
        history_prune(prune) {}
};

static const std::map<std::string, RawDataEntry> kRawData = {
    {id_ar, RawDataEntry(key_map_ar,
                         base::size(key_map_ar),
                         key_map_index_ar,
                         is_102_ar)},
    {id_ckb_ar, RawDataEntry(key_map_ckb_ar,
                             base::size(key_map_ckb_ar),
                             key_map_index_ckb_ar,
                             is_102_ckb_ar)},
    {id_ckb_en, RawDataEntry(key_map_ckb_en,
                             base::size(key_map_ckb_en),
                             key_map_index_ckb_en,
                             is_102_ckb_en)},
    {kIdDevaPhone, RawDataEntry(kKeyMapUs,
                                kKeyMapUsLen,
                                kKeyMapIndexUs,
                                kIs102Us,
                                kTransformsDevaPhone,
                                kTransformsDevaPhoneLen,
                                kHistoryPruneDevaPhone)},
    {id_fa, RawDataEntry(key_map_fa,
                         base::size(key_map_fa),
                         key_map_index_fa,
                         is_102_fa)},
    {id_km, RawDataEntry(key_map_km,
                         base::size(key_map_km),
                         key_map_index_km,
                         is_102_km)},
    {id_lo, RawDataEntry(key_map_lo,
                         base::size(key_map_lo),
                         key_map_index_lo,
                         is_102_lo)},
    {id_ne_inscript, RawDataEntry(key_map_ne_inscript,
                                  base::size(key_map_ne_inscript),
                                  key_map_index_ne_inscript,
                                  is_102_ne_inscript)},
    {id_ru_phone_aatseel, RawDataEntry(key_map_ru_phone_aatseel,
                                       base::size(key_map_ru_phone_aatseel),
                                       key_map_index_ru_phone_aatseel,
                                       is_102_ru_phone_aatseel)},
    {id_ru_phone_yazhert, RawDataEntry(key_map_ru_phone_yazhert,
                                       base::size(key_map_ru_phone_yazhert),
                                       key_map_index_ru_phone_yazhert,
                                       is_102_ru_phone_yazhert)},
    {id_ta_inscript, RawDataEntry(key_map_ta_inscript,
                                  base::size(key_map_ta_inscript),
                                  key_map_index_ta_inscript,
                                  is_102_ta_inscript)},
    {id_ta_typewriter, RawDataEntry(key_map_ta_typewriter,
                                    base::size(key_map_ta_typewriter),
                                    key_map_index_ta_typewriter,
                                    is_102_ta_typewriter)},
    {id_th, RawDataEntry(key_map_th,
                         base::size(key_map_th),
                         key_map_index_th,
                         is_102_th)},
    {id_th_pattajoti, RawDataEntry(key_map_th_pattajoti,
                                   base::size(key_map_th_pattajoti),
                                   key_map_index_th_pattajoti,
                                   is_102_th_pattajoti)},
    {id_th_tis, RawDataEntry(key_map_th_tis,
                             base::size(key_map_th_tis),
                             key_map_index_th_tis,
                             is_102_th_tis)}};

static const char* k101Keys[] = {
    // Row #1
    "BackQuote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
    "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
    // Row #2
    "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
    "KeyP", "BracketLeft", "BracketRight", "Backslash",
    // Row #3
    "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
    "Semicolon", "Quote",
    // Row #4
    "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period",
    "Slash",
    // Row #5
    "Space"};

static const char* k102Keys[] = {
    // Row #1
    "BackQuote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
    "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
    // Row #2
    "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
    "KeyP", "BracketLeft", "BracketRight",
    // Row #3
    "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
    "Semicolon", "Quote", "Backslash",
    // Row #4
    "IntlBackslash", "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM",
    "Comma", "Period", "Slash",
    // Row #5
    "Space"};

bool ScanBrackets(const wchar_t** pstr,
                  wchar_t bracket_left,
                  wchar_t bracket_right,
                  std::string* str_out) {
  const wchar_t* p = *pstr;
  if (*p != bracket_left || *(p + 1) != bracket_left)
    return false;
  p += 2;
  const wchar_t* from = p;
  while (*p != L'\0') {
    if (*p == bracket_right && *(p + 1) == bracket_right) {
      base::WideToUTF8(from, p - from, str_out);
      *pstr = p + 2;
      return true;
    }
    ++p;
  }
  return false;
}

// Parses the raw key map string and generate a KeyMap instance.
// Each character in the raw key map string maps to a key on the keyboard 101 or
// 102 layout with the same sequence of from top left to bottom right.
// For example, the 1st character maps to the BackQuote key, the 2nd maps to the
// Number1 key, and the last character maps to the Space key.
// Please refer to the sequences defined by |k101Keys| and |k102Keys|.
// If the definition wants to map multiple characters to one key, the double
// brackets are used. e.g. "{{abc}}" or "((abc))".
// The parsing supports both "{{}}" and "(())" to eliminate the ambiguities
// where it wants to map the character "{", "}", "(", or ")" to a key.
// e.g. "{{{abc}}..." is ambiguous and should be defined as "{((abc))..".
KeyMap ParseKeyMap(const wchar_t* raw_key_map, bool is_102) {
  const char** std_keys = is_102 ? k102Keys : k101Keys;
  KeyMap key_map;
  const wchar_t* p = raw_key_map;
  uint8_t index = 0;
  while (*p != L'\0') {
    std::string str;
    if (!ScanBrackets(&p, L'{', L'}', &str) &&
        !ScanBrackets(&p, L'(', L')', &str)) {
      base::WideToUTF8(p++, 1, &str);
    }

    DCHECK((is_102 && index < 49) || (!is_102 && index < 48));
    key_map[std_keys[index++]] = str;
  }
  return key_map;
}

// Parses the raw transform definition string and generate a transform rule map,
// and a merged regexp which is used to do the quick check whether a given
// string can match one of the transform rules.
// |raw_transforms| is a list of strings, the strings at the even number of
// index are the regexp to define the rule, and the strings at the odd number of
// index are the string to replace the matched string. It can contains "\\1",
// "\\2", etc. to represent the strings in the matched sub groups.
// e.g. this definition can swap the 2 digits when type "~":
//      "([0-9])([0-9])~" -> "\\2\\1"
std::unique_ptr<re2::RE2> ParseTransforms(
    const char** raw_transforms,
    uint16_t trans_count,
    std::map<uint16_t, TransformRule>& re_map) {
  if (!trans_count)
    return nullptr;

  DCHECK(!(trans_count & 1));

  std::string all_trans;
  uint16_t sum_of_groups = 1;
  for (uint16_t i = 0; i < trans_count; i += 2) {
    std::string from(raw_transforms[i]);
    const char* to = raw_transforms[i + 1];

    auto from_re = std::make_unique<re2::RE2>(from + "$");
    int group_count = from_re->NumberOfCapturingGroups();
    all_trans += "(" + from + "$)|";

    re_map[sum_of_groups] = std::make_pair(std::move(from_re), to);
    sum_of_groups += group_count + 1;
  }
  return std::make_unique<re2::RE2>(
      all_trans.substr(0, all_trans.length() - 1));
}

// Parses the history prune regexp and returns the RE2 instance.
// This regexp is used by the caller code instead of in RulesData.
std::unique_ptr<re2::RE2> ParseHistoryPrune(const char* history_prune) {
  if (!history_prune)
    return nullptr;
  return std::make_unique<re2::RE2>(std::string("^(") + history_prune + ")$");
}

// The delimit inserted at the position of "transat".
// The term "transat" means "was transformed at".
// Please refer to some details in the |Transform| method.
static const std::string kTransatDelimit = base::WideToUTF8(L"\u001D");

}  // namespace
KeyMap ParseKeyMapForTesting(const wchar_t* raw_key_map, bool is_102) {
  return ParseKeyMap(raw_key_map, is_102);
}

RulesData::RulesData() = default;
RulesData::~RulesData() = default;

// static
std::unique_ptr<RulesData> RulesData::Create(const wchar_t** key_map,
                                             const uint8_t key_map_count,
                                             const uint8_t* key_map_index,
                                             bool is_102_keyboard,
                                             const char** transforms,
                                             const uint16_t transforms_count,
                                             const char* history_prune) {
  std::unique_ptr<RulesData> data = std::make_unique<RulesData>();
  for (uint8_t i = 0; i < key_map_count; ++i)
    data->key_map_cache_.push_back(ParseKeyMap(key_map[i], is_102_keyboard));

  for (uint8_t i = 0; i < base::size(data->key_maps_); ++i) {
    uint8_t index = key_map_index[i];
    DCHECK(index < data->key_map_cache_.size());
    data->key_maps_[i] = &(data->key_map_cache_[index]);
  }
  data->transform_re_merged_ =
      ParseTransforms(transforms, transforms_count, data->transform_rules_);
  data->history_prune_re_ = ParseHistoryPrune(history_prune);
  return data;
}

// static
std::unique_ptr<RulesData> RulesData::GetById(const std::string& id) {
  auto it = kRawData.find(id);
  if (it == kRawData.end())
    return nullptr;

  const RawDataEntry& entry = it->second;
  return Create(entry.key_map, entry.key_map_count, entry.key_map_index,
                entry.is_102_keyboard, entry.transforms, entry.transforms_count,
                entry.history_prune);
}

// static
bool RulesData::IsIdSupported(const std::string& id) {
  return kRawData.find(id) != kRawData.end();
}

const KeyMap* RulesData::GetKeyMapByModifiers(uint8_t modifiers) const {
  return modifiers < 8 ? key_maps_[modifiers] : nullptr;
}

bool RulesData::Transform(const std::string& context,
                          int transat,
                          const std::string& appended,
                          std::string* transformed) const {
  // Inserts the transat delimit if |transat| indicates a valid position.
  // The rule definition may contains this delimit to explicit match a
  // transformed character. e.g. "a" -> "X", "X\u001Da" -> "aa". So when typing
  // "a", it will be transformed as "X". And typing "a" again can be transformed
  // as "aa". In that case, the caller must pass in the |transat| value as 1 in
  // order to match the regexp "X\u001Da". And if there was a "X" in the input
  // field, the caller passes |transat| as -1, therefore typing "a" won't
  // trigger the transform.
  std::string str = transat > 0 ? context.substr(0, transat) + kTransatDelimit +
                                      context.substr(transat) + appended
                                : context + appended;
  int nmatch = transform_re_merged_->NumberOfCapturingGroups();
  auto matches = std::make_unique<re2::StringPiece[]>(nmatch);
  bool is_matched = transform_re_merged_->Match(str, 0, str.length(),
                                                re2::RE2::Anchor::UNANCHORED,
                                                matches.get(), nmatch);
  if (!is_matched)
    return false;

  int pos = 1;
  while (pos < nmatch && !matches[pos].data())
    ++pos;

  const auto& found = transform_rules_.find(pos);
  if (found == transform_rules_.end())
    return false;

  auto& rule = found->second;
  auto& re = *(std::get<0>(rule));
  const std::string& repl = std::get<1>(rule);

  re2::RE2::Replace(&str, re, repl);
  re2::RE2::Replace(&str, "\u001d", "");
  *transformed = str;

  return true;
}

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos
