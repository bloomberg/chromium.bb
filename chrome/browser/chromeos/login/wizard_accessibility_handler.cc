// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_accessibility_handler.h"

#include <algorithm>

#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_accessibility_api_constants.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace keys = extension_accessibility_api_constants;

namespace {

static std::string SubstringUTF8(std::string str, int start, int len) {
  base::i18n::UTF8CharIterator iter(&str);
  for (int i = 0; i < start; i++) {
    if (!iter.Advance())
      return std::string();
  }

  int byte_start = iter.array_pos();
  for (int i = 0; i < len; i++) {
    if (!iter.Advance())
      break;
  }
  int byte_len = iter.array_pos() - byte_start;

  return str.substr(byte_start, byte_len);
}

// If the string consists of a single character and that character is
// punctuation that is not normally spoken by TTS, replace the string
// with a description of that character (like "period" for ".").
std::string DescribePunctuation(const std::string& str) {
  if (str == "!") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_EXCLAMATION_POINT);
  } else if (str == "(") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LEFT_PAREN);
  } else if (str == ")") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_RIGHT_PAREN);
  } else if (str == ";") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_SEMICOLON);
  } else if (str == ":") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_COLON);
  } else if (str == "\"") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_QUOTE);
  } else if (str == ",") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_COMMA);
  } else if (str == ".") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_PERIOD);
  } else if (str == " ") {
    return l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_SPACE);
  } else {
    return str;
  }
}

// Append words and separate adding a space if needed.  Call
// DescribePunctuation on to_append so that single punctuation
// characters are expanded ('.' -> 'period') but punctuation
// in the middle of a larger phrase are handled by the speech
// engine.
void AppendUtterance(std::string to_append, std::string* str) {
  if ((*str).size())
    *str += " ";

  *str += DescribePunctuation(to_append);
}

// Append a localized string from its message ID, adding a space if needed.
void AppendUtterance(int message_id, std::string* str) {
  AppendUtterance(l10n_util::GetStringUTF8(message_id), str);
}

// Append a phrase of the form "3 of 5", adding a space if needed.
void AppendIndexOfCount(int index, int count, std::string* str) {
  string16 index_str = base::IntToString16(index);
  string16 count_str = base::IntToString16(count);
  AppendUtterance(l10n_util::GetStringFUTF8(IDS_CHROMEOS_ACC_INDEX_OF_COUNT,
                                            index_str,
                                            count_str), str);
}

}  // anonymous namespace

namespace chromeos {

void WizardAccessibilityHandler::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  const AccessibilityControlInfo *control_info =
      Details<const AccessibilityControlInfo>(details).ptr();
  std::string description;
  EarconType earcon = NO_EARCON;
  DescribeAccessibilityEvent(type, control_info, &description, &earcon);
  Speak(description.c_str(), false, true);
}

void WizardAccessibilityHandler::Speak(const char* speak_str,
                                       bool queue,
                                       bool interruptible) {
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    if (queue || !interruptible) {
      std::string props = "";
      props.append("enqueue=");
      props.append(queue ? "1;" : "0;");
      props.append("interruptible=");
      props.append(interruptible ? "1;" : "0;");
      chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
          SetSpeakProperties(props.c_str());
    }
    chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
        Speak(speak_str);
  }
}

void WizardAccessibilityHandler::DescribeAccessibilityEvent(
    NotificationType event_type,
    const AccessibilityControlInfo* control_info,
    std::string* out_spoken_description,
    EarconType* out_earcon) {
  *out_spoken_description = std::string();
  *out_earcon = NO_EARCON;

  switch (event_type.value) {
    case NotificationType::ACCESSIBILITY_CONTROL_FOCUSED:
      DescribeControl(control_info, false, out_spoken_description, out_earcon);
      break;
    case NotificationType::ACCESSIBILITY_CONTROL_ACTION:
      DescribeControl(control_info, true, out_spoken_description, out_earcon);
      break;
    case NotificationType::ACCESSIBILITY_TEXT_CHANGED:
      DescribeTextChanged(control_info, out_spoken_description, out_earcon);
      break;
    case NotificationType::ACCESSIBILITY_MENU_OPENED:
      *out_earcon = EARCON_OBJECT_OPENED;
      break;
    case NotificationType::ACCESSIBILITY_MENU_CLOSED:
      *out_earcon = EARCON_OBJECT_CLOSED;
      break;
    default:
      NOTREACHED();
      return;
  }

  if (control_info->type() == keys::kTypeTextBox) {
    const AccessibilityTextBoxInfo* text_box =
        static_cast<const AccessibilityTextBoxInfo*>(control_info);
    previous_text_value_ = GetTextBoxValue(text_box);
    previous_text_selection_start_ = text_box->selection_start();
    previous_text_selection_end_ = text_box->selection_end();
  }
}

void WizardAccessibilityHandler::DescribeControl(
    const AccessibilityControlInfo* control_info,
    bool is_action,
    std::string* out_spoken_description,
    EarconType* out_earcon) {
  if (control_info->type() == keys::kTypeButton) {
    *out_earcon = EARCON_BUTTON;
    AppendUtterance(control_info->name(), out_spoken_description);
    AppendUtterance(IDS_CHROMEOS_ACC_BUTTON, out_spoken_description);
  } else if (control_info->type() == keys::kTypeCheckbox) {
    AppendUtterance(control_info->name(), out_spoken_description);
    const AccessibilityCheckboxInfo* checkbox_info =
        static_cast<const AccessibilityCheckboxInfo*>(control_info);
    if (checkbox_info->checked()) {
      *out_earcon = EARCON_CHECK_ON;
      AppendUtterance(IDS_CHROMEOS_ACC_CHECKBOX_CHECKED,
                      out_spoken_description);
    } else {
      *out_earcon = EARCON_CHECK_OFF;
      AppendUtterance(IDS_CHROMEOS_ACC_CHECKBOX_UNCHECKED,
                      out_spoken_description);
    }
  } else if (control_info->type() == keys::kTypeComboBox) {
    *out_earcon = EARCON_LISTBOX;
    const AccessibilityComboBoxInfo* combobox_info =
        static_cast<const AccessibilityComboBoxInfo*>(control_info);
    AppendUtterance(combobox_info->value(), out_spoken_description);
    AppendUtterance(combobox_info->name(), out_spoken_description);
    AppendUtterance(IDS_CHROMEOS_ACC_COMBOBOX, out_spoken_description);
    AppendIndexOfCount(combobox_info->item_index() + 1,
                       combobox_info->item_count(),
                       out_spoken_description);
  } else if (control_info->type() == keys::kTypeLink) {
    *out_earcon = EARCON_LINK;
    AppendUtterance(control_info->name(), out_spoken_description);
    AppendUtterance(IDS_CHROMEOS_ACC_LINK, out_spoken_description);
  } else if (control_info->type() == keys::kTypeListBox) {
    *out_earcon = EARCON_LISTBOX;
    const AccessibilityListBoxInfo* listbox_info =
        static_cast<const AccessibilityListBoxInfo*>(control_info);
    AppendUtterance(listbox_info->value(), out_spoken_description);
    AppendUtterance(listbox_info->name(), out_spoken_description);
    AppendUtterance(IDS_CHROMEOS_ACC_LISTBOX, out_spoken_description);
    AppendIndexOfCount(listbox_info->item_index() + 1,
                       listbox_info->item_count(),
                       out_spoken_description);
  } else if (control_info->type() == keys::kTypeMenu) {
    *out_earcon = EARCON_MENU;
    AppendUtterance(control_info->name(), out_spoken_description);
    AppendUtterance(IDS_CHROMEOS_ACC_MENU, out_spoken_description);
  } else if (control_info->type() == keys::kTypeMenuItem) {
    const AccessibilityMenuItemInfo* menu_item_info =
        static_cast<const AccessibilityMenuItemInfo*>(control_info);
    AppendUtterance(menu_item_info->name(), out_spoken_description);
    if (menu_item_info->has_submenu())
      AppendUtterance(IDS_CHROMEOS_ACC_HAS_SUBMENU, out_spoken_description);
    AppendIndexOfCount(menu_item_info->item_index() + 1,
                       menu_item_info->item_count(),
                       out_spoken_description);
  } else if (control_info->type() == keys::kTypeRadioButton) {
    AppendUtterance(control_info->name(), out_spoken_description);
    const AccessibilityRadioButtonInfo* radio_info =
        static_cast<const AccessibilityRadioButtonInfo*>(control_info);
    if (radio_info->checked()) {
      *out_earcon = EARCON_CHECK_ON;
      AppendUtterance(IDS_CHROMEOS_ACC_RADIO_SELECTED, out_spoken_description);
    } else {
      *out_earcon = EARCON_CHECK_OFF;
      AppendUtterance(IDS_CHROMEOS_ACC_RADIO_UNSELECTED,
                      out_spoken_description);
    }
    AppendIndexOfCount(radio_info->item_index() + 1,
                       radio_info->item_count(),
                       out_spoken_description);
  } else if (control_info->type() == keys::kTypeTab) {
    *out_earcon = EARCON_TAB;
    AppendUtterance(control_info->name(), out_spoken_description);
    const AccessibilityTabInfo* tab_info =
        static_cast<const AccessibilityTabInfo*>(control_info);
    AppendUtterance(IDS_CHROMEOS_ACC_TAB, out_spoken_description);
    AppendIndexOfCount(tab_info->tab_index() + 1,
                       tab_info->tab_count(),
                       out_spoken_description);
  } else if (control_info->type() == keys::kTypeTextBox) {
    *out_earcon = EARCON_TEXTBOX;
    const AccessibilityTextBoxInfo* textbox_info =
        static_cast<const AccessibilityTextBoxInfo*>(control_info);
    AppendUtterance(GetTextBoxValue(textbox_info), out_spoken_description);
    AppendUtterance(textbox_info->name(), out_spoken_description);
    if (textbox_info->password()) {
      AppendUtterance(IDS_CHROMEOS_ACC_PASSWORDBOX, out_spoken_description);
    } else {
      AppendUtterance(IDS_CHROMEOS_ACC_TEXTBOX, out_spoken_description);
    }
  } else if (control_info->type() == keys::kTypeWindow) {
    // No feedback when a window gets focus
  }

  if (is_action)
    AppendUtterance(IDS_CHROMEOS_ACC_SELECTED, out_spoken_description);
}

void WizardAccessibilityHandler::DescribeTextChanged(
    const AccessibilityControlInfo* control_info,
    std::string* out_spoken_description,
    EarconType* out_earcon) {
  DCHECK_EQ(control_info->type(), keys::kTypeTextBox);
  const AccessibilityTextBoxInfo* text_box =
      static_cast<const AccessibilityTextBoxInfo*>(control_info);

  std::string old_value = previous_text_value_;
  int old_start = previous_text_selection_start_;
  int old_end = previous_text_selection_end_;
  std::string new_value = GetTextBoxValue(text_box);
  int new_start = text_box->selection_start();
  int new_end = text_box->selection_end();

  if (new_value == old_value) {
    DescribeTextSelectionChanged(new_value,
                                 old_start, old_end,
                                 new_start, new_end,
                                 out_spoken_description);
  } else {
    DescribeTextContentsChanged(old_value, new_value,
                                out_spoken_description);
  }
}

std::string WizardAccessibilityHandler::GetTextBoxValue(
    const AccessibilityTextBoxInfo* textbox_info) {
  std::string value = textbox_info->value();
  if (textbox_info->password()) {
    base::i18n::UTF8CharIterator iter(&value);
    std::string obscured;
    while (!iter.end()) {
      obscured += "*";
      iter.Advance();
    }
    return obscured;
  } else {
    return value;
  }
}

void WizardAccessibilityHandler::DescribeTextSelectionChanged(
    const std::string& value,
    int old_start,
    int old_end,
    int new_start,
    int new_end,
    std::string* out_spoken_description) {
  if (new_start == new_end) {
    // It's currently a cursor.
    if (old_start != old_end) {
      // It was previously a selection, so just announce 'unselected'.
      AppendUtterance(IDS_CHROMEOS_ACC_TEXT_UNSELECTED, out_spoken_description);
    } else if (old_start == new_start + 1 || old_start == new_start - 1) {
      // Moved by one character; read it.
      AppendUtterance(SubstringUTF8(value, std::min(old_start, new_start), 1),
                      out_spoken_description);
    } else {
      // Moved by more than one character. Read all characters crossed.
      AppendUtterance(SubstringUTF8(value,
                                    std::min(old_start, new_start),
                                    abs(old_start - new_start)),
                      out_spoken_description);
    }
  } else {
    // It's currently a selection.
    if (old_start == old_end) {
      // It was previously a cursor.
      AppendUtterance(SubstringUTF8(value, new_start, new_end - new_start),
                      out_spoken_description);
    } else if (old_start == new_start && old_end < new_end) {
      // Added to end of selection.
      AppendUtterance(SubstringUTF8(value, old_end, new_end - old_end),
                      out_spoken_description);
    } else if (old_start == new_start && old_end > new_end) {
      // Removed from end of selection.
      AppendUtterance(SubstringUTF8(value, new_end, old_end - new_end),
                      out_spoken_description);
    } else if (old_end == new_end && old_start > new_start) {
      // Added to beginning of selection.
      AppendUtterance(SubstringUTF8(value, new_start, old_start - new_start),
                      out_spoken_description);
    } else if (old_end == new_end && old_start < new_start) {
      // Removed from beginning of selection.
      AppendUtterance(SubstringUTF8(value, old_start, new_start - old_start),
                      out_spoken_description);
    } else {
      // The selection changed but it wasn't an obvious extension of
      // a previous selection. Just read the new selection.
      AppendUtterance(SubstringUTF8(value, new_start, new_end - new_start),
                      out_spoken_description);
    }
  }
}

void WizardAccessibilityHandler::DescribeTextContentsChanged(
    const std::string& old_value,
    const std::string& new_value,
    std::string* out_spoken_description) {
  int old_array_len = old_value.size();
  int new_array_len = new_value.size();

  // Get the unicode characters and indices of the start of each
  // character's UTF8-encoded representation.
  scoped_array<int32> old_chars(new int32[old_array_len]);
  scoped_array<int> old_indices(new int[old_array_len + 1]);
  base::i18n::UTF8CharIterator old_iter(&old_value);
  while (!old_iter.end()) {
    old_chars[old_iter.char_pos()] = old_iter.get();
    old_indices[old_iter.char_pos()] = old_iter.array_pos();
    old_iter.Advance();
  }
  int old_char_len = old_iter.char_pos();
  old_indices[old_char_len] = old_iter.array_pos();

  scoped_array<int32> new_chars(new int32[new_array_len]);
  scoped_array<int> new_indices(new int[new_array_len + 1]);
  base::i18n::UTF8CharIterator new_iter(&new_value);
  while (!new_iter.end()) {
    new_chars[new_iter.char_pos()] = new_iter.get();
    new_indices[new_iter.char_pos()] = new_iter.array_pos();
    new_iter.Advance();
  }
  int new_char_len = new_iter.char_pos();
  new_indices[new_char_len] = new_iter.array_pos();

  // Find the common prefix of the two strings.
  int prefix_char_len = 0;
  while (prefix_char_len < old_char_len &&
         prefix_char_len < new_char_len &&
         old_chars[prefix_char_len] == new_chars[prefix_char_len]) {
    prefix_char_len++;
  }

  // Find the common suffix of the two stirngs.
  int suffix_char_len = 0;
  while (suffix_char_len < old_char_len - prefix_char_len &&
         suffix_char_len < new_char_len - prefix_char_len &&
         (old_chars[old_char_len - suffix_char_len - 1] ==
          new_chars[new_char_len - suffix_char_len - 1])) {
    suffix_char_len++;
  }

  int old_suffix_char_start = old_char_len - suffix_char_len;
  int new_suffix_char_start = new_char_len - suffix_char_len;

  // Find the substring that was deleted (if any) to get the new string
  // from the old - it's the part in the middle of the old string if you
  // remove the common prefix and suffix.
  std::string deleted = old_value.substr(
      old_indices[prefix_char_len],
      old_indices[old_suffix_char_start] - old_indices[prefix_char_len]);

  // Find the substring that was inserted (if any) to get the new string
  // from the old - it's the part in the middle of the new string if you
  // remove the common prefix and suffix.
  std::string inserted = new_value.substr(
      new_indices[prefix_char_len],
      new_indices[new_suffix_char_start] - new_indices[prefix_char_len]);

  if (!inserted.empty() && !deleted.empty()) {
    // Replace one substring with another, speak inserted text.
    AppendUtterance(inserted, out_spoken_description);
  } else if (!inserted.empty()) {
    // Speak inserted text.
    AppendUtterance(inserted, out_spoken_description);
  } else if (!deleted.empty()) {
    // Speak deleted text.
    AppendUtterance(deleted, out_spoken_description);
  }
}

}  // namespace chromeos
