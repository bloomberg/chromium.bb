// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_

#include "base/strings/string16.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace views {
class StyledLabel;
class Textfield;
}  // namespace views

namespace autofill {

// Defines a title view with an icon, a separator, and a label, to be used
// by dialogs that need to present the Google Pay logo with a separator and
// custom horizontal padding.
class TitleWithIconAndSeparatorView : public views::View {
 public:
  explicit TitleWithIconAndSeparatorView(const base::string16& window_title);
  ~TitleWithIconAndSeparatorView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// Creates and returns a small Textfield intended to be used for CVC entry.
views::Textfield* CreateCvcTextfield();

// Defines a view with legal message. This class handles the legal message
// parsing and the links clicking events.
class LegalMessageView : public views::View {
 public:
  explicit LegalMessageView(const LegalMessageLines& legal_message_lines,
                            views::StyledLabelListener* listener);
  ~LegalMessageView() override;

  void OnLinkClicked(views::StyledLabel* label,
                     const gfx::Range& range,
                     content::WebContents* web_contents);

 private:
  std::unique_ptr<views::StyledLabel> CreateLegalMessageLineLabel(
      const LegalMessageLine& line,
      views::StyledLabelListener* listener);

  LegalMessageLines legal_message_lines_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_
