// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

bool IsSureError(const autofill::ValidityMessage& message) {
  return message.sure && !message.text.empty();
}

}  // namespace

namespace autofill {

static const base::char16 kRangeSeparator = '|';

// Street address is multi-line, except in countries where it shares a line
// with other inputs (such as Coite d'Ivoire).
bool DetailInput::IsMultiline() const {
  return (type == ADDRESS_HOME_STREET_ADDRESS ||
          type == ADDRESS_BILLING_STREET_ADDRESS) &&
      length == DetailInput::LONG;
}

DialogNotification::DialogNotification() : type_(NONE) {}

DialogNotification::DialogNotification(Type type,
                                       const base::string16& display_text)
    : type_(type),
      display_text_(display_text),
      checked_(false) {
  // If there's a range separated by bars, mark that as the anchor text.
  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(display_text, kRangeSeparator, &pieces);
  if (pieces.size() > 1) {
    size_t start = pieces[0].size();
    size_t end = start + pieces[1].size();
    link_range_ = gfx::Range(start, end);
    display_text_ = JoinString(pieces, base::string16());
  }
}

DialogNotification::~DialogNotification() {}

SkColor DialogNotification::GetBackgroundColor() const {
  switch (type_) {
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(0xf5, 0xf5, 0xf5);
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
      return SkColorSetRGB(0xfc, 0xf3, 0xbf);
    case DialogNotification::DEVELOPER_WARNING:
    case DialogNotification::SECURITY_WARNING:
      return kWarningColor;
    case DialogNotification::NONE:
      return SK_ColorTRANSPARENT;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

SkColor DialogNotification::GetBorderColor() const {
  switch (type_) {
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(0xe5, 0xe5, 0xe5);
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::DEVELOPER_WARNING:
    case DialogNotification::SECURITY_WARNING:
    case DialogNotification::NONE:
      return GetBackgroundColor();
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

SkColor DialogNotification::GetTextColor() const {
  switch (type_) {
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(102, 102, 102);
    case DialogNotification::DEVELOPER_WARNING:
    case DialogNotification::SECURITY_WARNING:
      return SK_ColorWHITE;
    case DialogNotification::NONE:
      return SK_ColorTRANSPARENT;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

bool DialogNotification::HasArrow() const {
  return type_ == DialogNotification::WALLET_ERROR ||
         type_ == DialogNotification::WALLET_USAGE_CONFIRMATION;
}

bool DialogNotification::HasCheckbox() const {
  return type_ == DialogNotification::WALLET_USAGE_CONFIRMATION;
}

SkColor const kWarningColor = SkColorSetRGB(0xde, 0x49, 0x32);

SuggestionState::SuggestionState()
    : visible(false) {}
SuggestionState::SuggestionState(
    bool visible,
    const base::string16& vertically_compact_text,
    const base::string16& horizontally_compact_text,
    const gfx::Image& icon,
    const base::string16& extra_text,
    const gfx::Image& extra_icon)
    : visible(visible),
      vertically_compact_text(vertically_compact_text),
      horizontally_compact_text(horizontally_compact_text),
      icon(icon),
      extra_text(extra_text),
      extra_icon(extra_icon) {}
SuggestionState::~SuggestionState() {}

DialogOverlayString::DialogOverlayString() {}
DialogOverlayString::~DialogOverlayString() {}

DialogOverlayState::DialogOverlayState() {}
DialogOverlayState::~DialogOverlayState() {}

ValidityMessage::ValidityMessage(const base::string16& text, bool sure)
    : text(text), sure(sure) {}
ValidityMessage::~ValidityMessage() {}

ValidityMessages::ValidityMessages()
    : default_message_(ValidityMessage(base::string16(), false)) {}
ValidityMessages::~ValidityMessages() {}

void ValidityMessages::Set(
    ServerFieldType field, const ValidityMessage& message) {
  MessageMap::iterator iter = messages_.find(field);
  if (iter != messages_.end()) {
    if (!iter->second.text.empty())
      return;

    messages_.erase(iter);
  }

  messages_.insert(MessageMap::value_type(field, message));
}

const ValidityMessage& ValidityMessages::GetMessageOrDefault(
    ServerFieldType field) const {
  MessageMap::const_iterator iter = messages_.find(field);
  return iter != messages_.end() ? iter->second : default_message_;
}

bool ValidityMessages::HasSureError(ServerFieldType field) const {
  return IsSureError(GetMessageOrDefault(field));
}

bool ValidityMessages::HasErrors() const {
  for (MessageMap::const_iterator iter = messages_.begin();
       iter != messages_.end(); ++iter) {
    if (!iter->second.text.empty())
      return true;
  }
  return false;
}

bool ValidityMessages::HasSureErrors() const {
 for (MessageMap::const_iterator iter = messages_.begin();
      iter != messages_.end(); ++iter) {
    if (IsSureError(iter->second))
      return true;
  }
  return false;
}

}  // namespace autofill
