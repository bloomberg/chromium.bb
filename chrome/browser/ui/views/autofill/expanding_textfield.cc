// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/expanding_textfield.h"

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

namespace {

// The vertical padding between textfields.
const int kManualInputRowPadding = 10;

}  // namespace

// static
const char ExpandingTextfield::kViewClassName[] = "autofill/ExpandingTextfield";

ExpandingTextfield::ExpandingTextfield(
    const base::string16& default_value,
    const base::string16& placeholder,
    bool multiline,
    views::TextfieldController* controller)
    : controller_(controller) {
  textfields_.push_back(
      new DecoratedTextfield(base::string16(), placeholder, this));
  if (multiline) {
    textfields_.push_back(
        new DecoratedTextfield(base::string16(), placeholder, this));
  }
  SetText(default_value);

  for (std::list<DecoratedTextfield*>::iterator iter = textfields_.begin();
       iter != textfields_.end(); ++iter) {
    AddChildView(*iter);
  }

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        kManualInputRowPadding));
}

ExpandingTextfield::~ExpandingTextfield() {}

void ExpandingTextfield::SetText(const base::string16& text) {
  textfields_.front()->SetText(text);
  std::vector<base::string16> strings;
  base::SplitStringDontTrim(text, '\n', &strings);

  size_t i = 0;
  for (std::list<DecoratedTextfield*>::iterator iter = textfields_.begin();
       iter != textfields_.end(); ++iter) {
    (*iter)->SetText(i < strings.size() ? strings[i++] : base::string16());
  }

  for (; i < strings.size(); ++i) {
    textfields_.push_back(new DecoratedTextfield(
        strings[i],
        textfields_.front()->GetPlaceholderText(),
        this));
    AddChildView(textfields_.back());
    PreferredSizeChanged();
  }
}

base::string16 ExpandingTextfield::GetText() {
  base::string16 text = textfields_.front()->text();
  std::list<DecoratedTextfield*>::const_iterator iter = ++textfields_.begin();
  while (iter != textfields_.end()) {
    text += base::ASCIIToUTF16("\n") + (*iter++)->text();
  }
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  return text;
}

void ExpandingTextfield::SetInvalid(bool invalid) {
  textfields_.front()->SetInvalid(invalid);
}

void ExpandingTextfield::SetDefaultWidthInCharacters(int chars) {
  ForEachTextfield(&DecoratedTextfield::set_default_width_in_chars, chars);
}

void ExpandingTextfield::SetPlaceholderText(const base::string16& placeholder) {
  ForEachTextfield<views::Textfield, const base::string16&>(
      &DecoratedTextfield::set_placeholder_text, placeholder);
}

void ExpandingTextfield::SetIcon(const gfx::Image& icon) {
  textfields_.front()->SetIcon(icon);
}

void ExpandingTextfield::SetTooltipIcon(const base::string16& text) {
  textfields_.front()->SetTooltipIcon(text);
}

void ExpandingTextfield::SetEditable(bool editable) {
  ForEachTextfield(&DecoratedTextfield::SetEditable, editable);
}

const char* ExpandingTextfield::GetClassName() const {
  return kViewClassName;
}

void ExpandingTextfield::ContentsChanged(views::Textfield* sender,
                                         const base::string16& new_contents) {
  if (textfields_.size() > 1 && sender == textfields_.back() &&
      !new_contents.empty()) {
    textfields_.push_back(
        new DecoratedTextfield(base::string16(),
                               sender->GetPlaceholderText(),
                               this));
    AddChildView(textfields_.back());
    PreferredSizeChanged();
  }

  controller_->ContentsChanged(sender, new_contents);
}

bool ExpandingTextfield::HandleKeyEvent(views::Textfield* sender,
                                        const ui::KeyEvent& key_event) {
  return controller_->HandleKeyEvent(sender, key_event);
}

bool ExpandingTextfield::HandleMouseEvent(views::Textfield* sender,
                                          const ui::MouseEvent& mouse_event) {
  return controller_->HandleMouseEvent(sender, mouse_event);
}

template <typename BaseType, typename Param>
void ExpandingTextfield::ForEachTextfield(
    void (BaseType::* f)(Param), Param p) const {
  for (std::list<DecoratedTextfield*>::const_iterator iter =
           textfields_.begin();
       iter != textfields_.end(); ++iter) {
    base::Bind(f, base::Unretained(*iter), p).Run();
  }
}

}  // namespace autofill
