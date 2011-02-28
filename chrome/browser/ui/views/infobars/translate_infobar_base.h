// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_INFOBAR_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_INFOBAR_BASE_H_
#pragma once

#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/translate_infobar_view.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"

class TranslateInfoBarDelegate;

namespace views {
class MenuButton;
}

// This class contains some of the base functionality that translate infobars
// use.
class TranslateInfoBarBase : public TranslateInfoBarView,
                             public InfoBarView {
 public:
  explicit TranslateInfoBarBase(TranslateInfoBarDelegate* delegate);
  virtual ~TranslateInfoBarBase();

 protected:
  static const int kButtonInLabelSpacing;

  // Sets the text of the provided language menu button to reflect the current
  // value from the delegate.
  void UpdateLanguageButtonText(views::MenuButton* button,
                                LanguagesMenuModel::LanguageType language);

  // Convenience to retrieve the TranslateInfoBarDelegate for this infobar.
  TranslateInfoBarDelegate* GetDelegate();

 private:
  // InfoBarView:
  virtual void OnPaintBackground(gfx::Canvas* canvas);
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Returns the background that should be displayed when not animating.
  const views::Background& GetBackground();

  // Paints |background| to |canvas| with the opacity level based on
  // |animation_value|.
  void FadeBackground(gfx::Canvas* canvas,
                      double animation_value,
                      const views::Background& background);

  InfoBarBackground normal_background_;
  InfoBarBackground error_background_;
  scoped_ptr<ui::SlideAnimation> background_color_animation_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_INFOBAR_BASE_H_
