// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_INFOBARS_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_INFOBARS_H_

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/browser/views/infobars/infobars.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/translate_errors.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class MenuButton;
}
class LanguagesMenuModel;
class OptionsMenuModel;
class TranslateTextButton;

// This file contains implementations for infobars for the Translate feature.

class TranslateInfoBar : public InfoBar,
                         public views::ViewMenuDelegate,
                         public menus::SimpleMenuModel::Delegate,
                         public NotificationObserver {
 public:
  explicit TranslateInfoBar(TranslateInfoBarDelegate* delegate);
  virtual ~TranslateInfoBar();

  // Overridden from views::View:
  virtual void Layout();
  virtual void PaintBackground(gfx::Canvas* canvas);

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
      menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
      const NotificationSource& source, const NotificationDetails& details);

 protected:
  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);

 private:
  void CreateLabels();
  views::Label* CreateLabel(const std::wstring& label);
  views::MenuButton* CreateMenuButton(int menu_id, const std::wstring& label,
      bool normal_has_border);
  gfx::Point DetermineMenuPositionAndAlignment(views::MenuButton* menu_button,
      views::Menu2::Alignment* alignment);
  void UpdateState(TranslateInfoBarDelegate::TranslateState new_state,
      bool translation_pending, TranslateErrors::Type error_type);
  void OnLanguageModified(views::MenuButton* menu_button,
      int new_language_index);
  void FadeBackground(gfx::Canvas* canvas, double animation_value,
      TranslateInfoBarDelegate::TranslateState state);
  inline TranslateInfoBarDelegate* GetDelegate() const;
  inline InfoBarBackground* GetBackground(
      TranslateInfoBarDelegate::TranslateState new_state) const;
  inline int GetSpacingAfterFirstLanguageButton() const;

  // Infobar keeps track of the state it is displaying, which should match that
  // in the TranslateInfoBarDelegate.  UI needs to keep track separately because
  // infobar may receive PAGE_TRANSLATED notifications before delegate does, in
  // which case, delegate's state is not updated and hence can't be used to
  // update display.  After the notification is sent out to all observers, both
  // infobar and delegate would end up with the same state.
  TranslateInfoBarDelegate::TranslateState state_;
  bool translation_pending_;
  TranslateErrors::Type error_type_;

  views::ImageView* icon_;
  views::Label* label_1_;
  views::Label* label_2_;
  views::Label* label_3_;
  views::Label* translating_label_;
  views::Label* error_label_;
  TranslateTextButton* accept_button_;
  TranslateTextButton* deny_button_;
  views::MenuButton* original_language_menu_button_;
  views::MenuButton* target_language_menu_button_;
  TranslateTextButton* revert_button_;
  views::MenuButton* options_menu_button_;
  TranslateTextButton* retry_button_;

  scoped_ptr<LanguagesMenuModel> original_language_menu_model_;
  scoped_ptr<LanguagesMenuModel> target_language_menu_model_;
  scoped_ptr<OptionsMenuModel> options_menu_model_;

  scoped_ptr<views::Menu2> original_language_menu_menu_;
  scoped_ptr<views::Menu2> target_language_menu_menu_;
  scoped_ptr<views::Menu2> options_menu_menu_;

  // This is true if language placeholders in label have been swapped.
  bool swapped_language_placeholders_;

  NotificationRegistrar notification_registrar_;

  scoped_ptr<InfoBarBackground> normal_background_;
  scoped_ptr<InfoBarBackground> error_background_;
  scoped_ptr<SlideAnimation> error_animation_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBar);
};

#endif  // CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_INFOBARS_H_
