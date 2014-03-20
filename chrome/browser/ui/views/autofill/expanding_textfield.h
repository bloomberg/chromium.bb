// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_EXPANDING_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_EXPANDING_TEXTFIELD_H_

#include <list>

#include "base/strings/string16.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace gfx {
class Image;
}

namespace autofill {

// A view that houses a stack of textfields. The stack grows as needed.
class ExpandingTextfield : public views::View,
                           public views::TextfieldController {
 public:
  static const char kViewClassName[];

  // When |multiline| is false, the view acts pretty much like a normal
  // DecoratedTextfield.
  ExpandingTextfield(const base::string16& default_value,
                     const base::string16& placeholder,
                     bool multiline,
                     views::TextfieldController* controller);
  virtual ~ExpandingTextfield();

  // Sets the contents of the textfields. Textfield n is set to the nth line
  // of |text|, as separated by line returns.
  void SetText(const base::string16& text);
  // Concatenates text contents of all textfields (with line returns as the
  // joining character) and returns it.
  base::string16 GetText();

  // Sets whether to indicate the first textfield has invalid content. Latter
  // textfields are always valid.
  void SetInvalid(bool invalid);
  bool invalid() {
    return textfields_.front()->invalid();
  }

  // Like validity, this only cares about the first textfield.
  void SetEditable(bool editable);
  bool editable() {
    return textfields_.front()->editable();
  }

  // DecoratedTextfield pass-throughs.
  void SetDefaultWidthInCharacters(int chars);
  void SetPlaceholderText(const base::string16& placeholder);
  void SetIcon(const gfx::Image& icon);
  void SetTooltipIcon(const base::string16& text);

  // View implementation.
  virtual const char* GetClassName() const OVERRIDE;
  using views::View::needs_layout;

  // TextfieldController implementation.
  virtual void ContentsChanged(views::Textfield* sender,
                               const base::string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;
  virtual bool HandleMouseEvent(views::Textfield* sender,
                                const ui::MouseEvent& mouse_event) OVERRIDE;

 private:
  // Calls a given function on every textfield.
  template <typename BaseType, typename Param>
  void ForEachTextfield(void (BaseType::* f)(Param), Param p) const;

  // The list of textfields. Owned as child views.
  std::list<DecoratedTextfield*> textfields_;

  TextfieldController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ExpandingTextfield);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_EXPANDING_TEXTFIELD_H_
