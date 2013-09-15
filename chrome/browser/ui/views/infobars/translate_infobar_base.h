// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_INFOBAR_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_INFOBAR_BASE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"

class TranslateInfoBarDelegate;

namespace views {
class MenuButton;
}

// This class contains some of the base functionality that translate infobars
// use.
class TranslateInfoBarBase : public InfoBarView {
 public:
  // Sets the text of the provided language menu button.
  void UpdateLanguageButtonText(views::MenuButton* button,
                                const string16& text);

 protected:
  TranslateInfoBarBase(InfoBarService* owner,
                       TranslateInfoBarDelegate* delegate);
  virtual ~TranslateInfoBarBase();

  // InfoBarView:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  // Convenience to retrieve the TranslateInfoBarDelegate for this infobar.
  TranslateInfoBarDelegate* GetDelegate();

  static const int kButtonInLabelSpacing;

 private:
  // InfoBarView:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // Returns the background that should be displayed when not animating.
  const views::Background& GetBackground();

  // Paints |background| to |canvas| with the opacity level based on
  // |animation_value|.
  void FadeBackground(gfx::Canvas* canvas,
                      double animation_value,
                      const views::Background& background);

  InfoBarBackground error_background_;
  scoped_ptr<gfx::SlideAnimation> background_color_animation_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_INFOBAR_BASE_H_
