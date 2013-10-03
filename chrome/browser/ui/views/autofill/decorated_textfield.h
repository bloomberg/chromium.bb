// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield.h"

namespace views {
class FocusableBorder;
class TextfieldController;
}

namespace autofill {

// A class which holds a textfield and draws extra stuff on top, like
// invalid content indications.
class DecoratedTextfield : public views::Textfield {
 public:
  static const char kViewClassName[];

  DecoratedTextfield(const base::string16& default_value,
                     const base::string16& placeholder,
                     views::TextfieldController* controller);
  virtual ~DecoratedTextfield();

  // Sets whether to indicate the textfield has invalid content.
  void SetInvalid(bool invalid);
  bool invalid() const { return invalid_; }

  // Sets the icon to be displayed inside the textfield at the end of the
  // text.
  void SetIcon(const gfx::Image& icon);

  // Sets a tooltip for this field. This will override the icon set with
  // SetIcon(), if any, and will be overridden by future calls to SetIcon().
  void SetTooltipIcon(const base::string16& text);

  // views::View implementation.
  virtual const char* GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DecoratedTextfieldTest, HeightMatchesButton);

  // Called to update the layout after SetIcon or SetTooltipIcon has been
  // called.
  void IconChanged();

  // This number corresponds to the number of pixels in the images that
  // are used to draw a views button which are above or below the actual border.
  // This number is encoded in the button assets themselves, so there's no other
  // way to get it than to hardcode it here.
  static const int kMagicInsetNumber;

  // We draw the border.
  views::FocusableBorder* border_;  // Weak.

  // The view that holds the icon at the end of the textfield.
  scoped_ptr<views::ImageView> icon_view_;

  // Whether the text contents are "invalid" (i.e. should special markers be
  // shown to indicate invalidness).
  bool invalid_;

  DISALLOW_COPY_AND_ASSIGN(DecoratedTextfield);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_
