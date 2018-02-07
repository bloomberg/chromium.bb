// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/text_input_info.h"

namespace vr {

TextInputInfo::TextInputInfo()
    : selection_start(0),
      selection_end(0),
      composition_start(kDefaultCompositionIndex),
      composition_end(kDefaultCompositionIndex) {}

TextInputInfo::TextInputInfo(base::string16 t)
    : text(t),
      selection_start(t.length()),
      selection_end(t.length()),
      composition_start(kDefaultCompositionIndex),
      composition_end(kDefaultCompositionIndex) {}

TextInputInfo::TextInputInfo(const TextInputInfo& other)
    : text(other.text),
      selection_start(other.selection_start),
      selection_end(other.selection_end),
      composition_start(other.composition_start),
      composition_end(other.composition_end) {}

bool TextInputInfo::operator==(const TextInputInfo& other) const {
  return text == other.text && selection_start == other.selection_start &&
         selection_end == other.selection_end &&
         composition_start == other.composition_start &&
         composition_end == other.composition_end;
}

bool TextInputInfo::operator!=(const TextInputInfo& other) const {
  return !(*this == other);
}

size_t TextInputInfo::SelectionSize() const {
  return std::abs(selection_end - selection_start);
}

EditedText::EditedText() {}

EditedText::EditedText(const EditedText& other)
    : current(other.current), previous(other.previous) {}

EditedText::EditedText(const TextInputInfo& new_current,
                       const TextInputInfo& new_previous)
    : current(new_current), previous(new_previous) {}

EditedText::EditedText(base::string16 t) : current(t) {}

bool EditedText::operator==(const EditedText& other) const {
  return current == other.current && previous == other.previous;
}

void EditedText::Update(const TextInputInfo& info) {
  previous = current;
  current = info;
}

std::string EditedText::ToString() const {
  return current.ToString() + ", previously " + previous.ToString();
}

static_assert(sizeof(base::string16) + 16 == sizeof(TextInputInfo),
              "If new fields are added to TextInputInfo, we must explicitly "
              "bump this size and update operator==");

}  // namespace vr
