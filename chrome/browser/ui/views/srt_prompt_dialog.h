// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SRT_PROMPT_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SRT_PROMPT_DIALOG_H_

#include <memory>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace gfx {
class SlideAnimation;
}

namespace safe_browsing {
class SRTPromptController;
}

// A modal dialog asking the user if they want to run the Chrome Cleanup
// tool. The dialog will have the following sections:
//
// 1. Main section with general information about unwanted software that has
// been found on the user's system.
// 2. Expandable details section with more details about unwanted software that
// will be removed and Chrome settings that will be reset.
// 3. Checkbox asking for permissions to upload logs (not yet implemented).
//
// The strings and icons used in the dialog are provided by a
// |SRTPromptController| object, which will also receive information about how
// the user interacts with the dialog. The controller object owns itself and
// will delete itself once it has received information about the user's
// interaction with the dialog. See the |SRTPromptController| class's
// description for more details.
class SRTPromptDialog : public views::DialogDelegateView,
                        public views::ButtonListener,
                        public gfx::AnimationDelegate {
 public:
  // The |controller| object manages its own lifetime and is not owned by
  // |SRTPromptDialog|. See the description of the |SRTPromptController| class
  // for details.
  explicit SRTPromptDialog(safe_browsing::SRTPromptController* controller);
  ~SRTPromptDialog() override;

  void Show(Browser* browser);

  // ui::DialogModel overrides.
  bool ShouldDefaultButtonBeBlue() const override;

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate overrides.
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;

  // views::ButtonListener overrides.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // gfx::AnimationDelegate overrides.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  class ExpandableMessageView;

  SkColor GetDetailsButtonColor();
  void UpdateDetailsButton();

  Browser* browser_;
  // The pointer will be set to nullptr once the controller has been notified of
  // user interaction since the controller can delete itself after that point.
  safe_browsing::SRTPromptController* controller_;

  std::unique_ptr<gfx::SlideAnimation> slide_animation_;
  ExpandableMessageView* details_view_;
  views::LabelButton* details_button_;

  DISALLOW_COPY_AND_ASSIGN(SRTPromptDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SRT_PROMPT_DIALOG_H_
