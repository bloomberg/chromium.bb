// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TRANSLATE_TRANSLATE_INFOBAR_BASE_GTK_H_
#define CHROME_BROWSER_UI_GTK_TRANSLATE_TRANSLATE_INFOBAR_BASE_GTK_H_
#pragma once

#include "chrome/browser/translate/translate_infobar_view.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "ui/base/animation/animation_delegate.h"

class MenuGtk;
class OptionsMenuModel;
class TranslateInfoBarDelegate;

// This class contains some of the base functionality that translate infobars
// use.
class TranslateInfoBarBase : public TranslateInfoBarView,
                             public InfoBar,
                             public ui::AnimationDelegate {
 public:
  explicit TranslateInfoBarBase(TranslateInfoBarDelegate* delegate);
  virtual ~TranslateInfoBarBase();

  // Initializes the infobar widgets. Should be called after the object has been
  // created.
  virtual void Init();

  // Overridden from InfoBar:
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double *b);
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double *b);

  // Overridden from TranslateInfoBarView:
  virtual void OriginalLanguageChanged() {}
  virtual void TargetLanguageChanged() {}

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation);

 protected:
  // Sub-classes that want to have the options menu button showing sould
  // override and return true.
  virtual bool ShowOptionsMenuButton() const;

  // Creates a label with the appropriate font and color for the translate
  // infobars.
  GtkWidget* CreateLabel(const std::string& text);

  // Creates a combobox that displays the languages currently available.
  // |selected_language| is the language index (as used in the
  // TranslateInfoBarDelegate) that should be selected initially.
  // |exclude_language| is the language index of the language that should not be
  // included in the list (-1 means no language excluded).
  GtkWidget* CreateLanguageCombobox(int selected_language,
                                    int exclude_language);

  // Given an above-constructed combobox, returns the currently selected
  // language id.
  static int GetLanguageComboboxActiveId(GtkComboBox* combo);

  // Convenience to retrieve the TranslateInfoBarDelegate for this infobar.
  TranslateInfoBarDelegate* GetDelegate() const;

 private:
  // Builds a button with an arrow in it to emulate the menu-button style from
  // the windows version.
  static GtkWidget* BuildOptionsMenuButton();

  // The menu displayed when the Options button is pressed.
  scoped_ptr<OptionsMenuModel> options_menu_model_;
  scoped_ptr<MenuGtk> options_menu_menu_;

  CHROMEGTK_CALLBACK_0(TranslateInfoBarBase, void, OnOptionsClicked);

  // A percentage to average the normal page action background with the error
  // background. When 0, the infobar background should be pure PAGE_ACTION_TYPE.
  // When 1, the infobar background should be pure WARNING_TYPE.
  double background_error_percent_;

  // Changes the color of the background from normal to error color and back.
  scoped_ptr<ui::SlideAnimation> background_color_animation_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarBase);
};

#endif  // CHROME_BROWSER_UI_GTK_TRANSLATE_TRANSLATE_INFOBAR_BASE_GTK_H_
