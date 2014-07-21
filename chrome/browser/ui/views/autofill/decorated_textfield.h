// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class ImageView;
class TextfieldController;
}

namespace autofill {

// A class which holds a textfield and draws extra stuff on top, like
// invalid content indications.
class DecoratedTextfield : public views::Textfield,
                           public views::ViewTargeterDelegate {
 public:
  static const char kViewClassName[];

  DecoratedTextfield(const base::string16& default_value,
                     const base::string16& placeholder,
                     views::TextfieldController* controller);
  virtual ~DecoratedTextfield();

  // Sets whether to indicate the textfield has invalid content.
  void SetInvalid(bool invalid);
  bool invalid() const { return invalid_; }

  // See docs for |editable_|.
  void SetEditable(bool editable);
  bool editable() const { return editable_; }

  // Sets the icon to display inside the textfield at the end of the text.
  void SetIcon(const gfx::Image& icon);

  // Sets a tooltip for this field. This will override the icon set with
  // SetIcon(), if any, and will be overridden by future calls to SetIcon().
  void SetTooltipIcon(const base::string16& text);

  // views::Textfield implementation.
  virtual base::string16 GetPlaceholderText() const OVERRIDE;

  // views::View implementation.
  virtual const char* GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DecoratedTextfieldTest, HeightMatchesButton);

  // views::ViewTargeterDelegate:
  virtual views::View* TargetForRect(views::View* root,
                                     const gfx::Rect& rect) OVERRIDE;

  // Updates the background after its color may have changed.
  void UpdateBackground();

  // Updates the border after its color or insets may have changed.
  void UpdateBorder();

  // Called to update the layout after SetIcon or SetTooltipIcon was called.
  void IconChanged();

  // The view that holds the icon at the end of the textfield.
  scoped_ptr<views::ImageView> icon_view_;

  // Whether the text contents are "invalid" (i.e. should special markers be
  // shown to indicate invalidness).
  bool invalid_;

  // Whether the user can edit the field. When not editable, many of the
  // pieces of the textfield disappear (border, background, icon, placeholder
  // text) and it can't receive focus.
  bool editable_;

  DISALLOW_COPY_AND_ASSIGN(DecoratedTextfield);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_
