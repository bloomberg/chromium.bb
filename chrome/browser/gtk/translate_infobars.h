// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TRANSLATE_INFOBARS_H_
#define CHROME_BROWSER_GTK_TRANSLATE_INFOBARS_H_

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "app/menus/simple_menu_model.h"
#include "chrome/browser/gtk/infobar_gtk.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/common/notification_registrar.h"

class SlideAnimation;
class OptionsMenuModel;

// InfoBar that asks user if they want to translate a page that isn't in the
// user's language to their language. Bar changes during and after translation.
class TranslateInfoBar : public InfoBar,
                         public AnimationDelegate,
                         public menus::SimpleMenuModel::Delegate,
                         public MenuGtk::Delegate {
 public:
  explicit TranslateInfoBar(TranslateInfoBarDelegate* delegate);
  virtual ~TranslateInfoBar();

  // Overridden from InfoBar:
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double *b);
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double *b);

  // Overridden from NotificationObserver (through InfoBar):
  virtual void Observe(NotificationType type,
      const NotificationSource& source, const NotificationDetails& details);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);

  // Overridden for both menus::SimpleMenuModel::Delegate and MenuGtk::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool GetAcceleratorForCommandId(int command_id,
      menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Overridden from MenuGtk::Delegate:
  virtual void ExecuteCommandById(int command_id) {
    ExecuteCommand(command_id);
  }

 private:
  // Builds all the widgets and sets the initial view of the dialog.
  void BuildWidgets();

  // Changes the layout of the info bar, displaying the correct widgets in the
  // correct order for |new_state|.
  void UpdateState(TranslateInfoBarDelegate::TranslateState new_state,
      bool new_translation_pending, TranslateErrors::Type error_type);

  // Sets the text in the three labels for the current state.
  void SetLabels();

  // Casts delegate() to the only possible subclass.
  TranslateInfoBarDelegate* GetDelegate() const;

  // Called after OnOriginalModified or OnTargetModified.
  void LanguageModified();

  CHROMEGTK_CALLBACK_0(TranslateInfoBar, void, OnOriginalModified);
  CHROMEGTK_CALLBACK_0(TranslateInfoBar, void, OnTargetModified);
  CHROMEGTK_CALLBACK_0(TranslateInfoBar, void, OnAcceptPressed);
  CHROMEGTK_CALLBACK_0(TranslateInfoBar, void, OnDenyPressed);
  CHROMEGTK_CALLBACK_0(TranslateInfoBar, void, OnRevertPressed);
  CHROMEGTK_CALLBACK_0(TranslateInfoBar, void, OnOptionsClicked);

  // A percentage to average the normal page action background with the error
  // background. When 0, the infobar background should be pure
  // PAGE_ACTION_TYPE. When 1, the infobar background should be pure
  // ERROR_TYPE.
  double background_error_percent_;

  // Infobar keeps track of the state it is displaying, which should match that
  // in the TranslateInfoBarDelegate.  UI needs to keep track separately because
  // infobar may receive PAGE_TRANSLATED notifications before delegate does, in
  // which case, delegate's state is not updated and hence can't be used to
  // update display.  After the notification is sent out to all observers, both
  // infobar and delegate would end up with the same state.
  TranslateInfoBarDelegate::TranslateState state_;
  bool translation_pending_;
  TranslateErrors::Type error_type_;

  GtkWidget* translate_box_;
  GtkWidget* label_1_;
  GtkWidget* label_2_;
  GtkWidget* label_3_;
  GtkWidget* translating_label_;
  GtkWidget* error_label_;

  GtkWidget* accept_button_;
  GtkWidget* accept_button_vbox_;

  GtkWidget* deny_button_;
  GtkWidget* deny_button_vbox_;

  GtkWidget* retry_button_;
  GtkWidget* retry_button_vbox_;

  GtkWidget* revert_button_;
  GtkWidget* revert_button_vbox_;

  GtkWidget* original_language_combobox_;
  GtkWidget* original_language_combobox_vbox_;

  GtkWidget* target_language_combobox_;
  GtkWidget* target_language_combobox_vbox_;

  GtkWidget* options_menu_button_;
  scoped_ptr<MenuGtk> options_menu_menu_;
  scoped_ptr<OptionsMenuModel> options_menu_model_;

  // This is true if language placeholders in label have been swapped.
  bool swapped_language_placeholders_;

  NotificationRegistrar notification_registrar_;

  // Changes the color of the background from blue to red and back.
  scoped_ptr<SlideAnimation> error_animation_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBar);
};

#endif  // CHROME_BROWSER_GTK_TRANSLATE_INFOBARS_H_
