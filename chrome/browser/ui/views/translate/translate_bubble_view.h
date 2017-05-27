// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/language_combobox_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/styled_label_listener.h"

class Browser;

namespace views {
class Checkbox;
class LabelButton;
class View;
}

namespace ui {
class SimpleComboboxModel;
}

class TranslateBubbleView : public LocationBarBubbleDelegateView,
                            public views::ButtonListener,
                            public views::ComboboxListener,
                            public views::LinkListener,
                            public views::MenuButtonListener,
                            public ui::SimpleMenuModel::Delegate,
                            public views::StyledLabelListener,
                            public content::WebContentsObserver {
 public:
  // Commands shown in the action-style combobox. The value corresponds to the
  // position in the combobox menu. Gaps will become separators.
  enum class DenialComboboxIndex {
    DONT_TRANSLATE = 0,
    NEVER_TRANSLATE_LANGUAGE = 1,
    NEVER_TRANSLATE_SITE = 3,
    MENU_SIZE = 4,
  };

  // Item IDs for the denial button's menu.
  enum DenialMenuItem { NEVER_TRANSLATE_LANGUAGE, NEVER_TRANSLATE_SITE };

  ~TranslateBubbleView() override;

  // Shows the Translate bubble. Returns the newly created bubble's Widget or
  // nullptr in cases when the bubble already exists or when the bubble is not
  // created.
  //
  // |is_user_gesture| is true when the bubble is shown on the user's deliberate
  // action.
  static views::Widget* ShowBubble(views::View* anchor_view,
                                   content::WebContents* web_contents,
                                   translate::TranslateStep step,
                                   translate::TranslateErrors::Type error_type,
                                   DisplayReason reason);

  // Closes the current bubble if it exists.
  static void CloseCurrentBubble();

  // Returns the bubble view currently shown. This may return NULL.
  static TranslateBubbleView* GetCurrentBubble();

  TranslateBubbleModel* model() { return model_.get(); }

  // views::BubbleDialogDelegateView methods.
  void Init() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetDelegate methods.
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // views::View methods.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ComboboxListener methods.
  void OnPerformAction(views::Combobox* combobox) override;

  // views::LinkListener method.
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::MenuButtonListener method.
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  // ui::SimpleMenuModel::Delegate methods.
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::StyledLabelListener method.
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // content::WebContentsObserver method.
  void WebContentsDestroyed() override;

  // Overridden from views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // Returns the current view state.
  TranslateBubbleModel::ViewState GetViewState() const;

 protected:
  // LocationBarBubbleDelegateView:
  void CloseBubble() override;

 private:
  enum LinkID {
    LINK_ID_ADVANCED,
    LINK_ID_LANGUAGE_SETTINGS,
  };

  enum ButtonID {
    BUTTON_ID_TRANSLATE,
    BUTTON_ID_DONE,
    BUTTON_ID_CANCEL,
    BUTTON_ID_SHOW_ORIGINAL,
    BUTTON_ID_TRY_AGAIN,
    BUTTON_ID_ALWAYS_TRANSLATE,
  };

  enum ComboboxID {
    COMBOBOX_ID_DENIAL,
    COMBOBOX_ID_SOURCE_LANGUAGE,
    COMBOBOX_ID_TARGET_LANGUAGE,
  };

  friend class TranslateBubbleViewTest;
  friend void ::translate::test_utils::PressTranslate(::Browser*);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TranslateButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TranslateButtonIn2016Q2UI);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, CloseButtonIn2016Q2UI);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, AdvancedLink);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, AdvancedLinkIn2016Q2UI);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, ShowOriginalButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TryAgainButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndCancelButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, DoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           DoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           CancelButtonReturningBeforeTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           CancelButtonReturningAfterTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, CancelButtonReturningError);

  TranslateBubbleView(views::View* anchor_view,
                      std::unique_ptr<TranslateBubbleModel> model,
                      translate::TranslateErrors::Type error_type,
                      content::WebContents* web_contents);

  // Returns the current child view.
  views::View* GetCurrentView() const;

  // Handles the event when the user presses a button.
  void HandleButtonPressed(ButtonID sender_id);

  // Handles the event when the user clicks a link.
  void HandleLinkClicked(LinkID sender_id);

  // Handles the event when the user changes an index of a combobox.
  void HandleComboboxPerformAction(ComboboxID sender_id);

  // Updates the visibilities of child views according to the current view type.
  void UpdateChildVisibilities();

  // Creates the 'before translate' view. Caller takes ownership of the returned
  // view.
  views::View* CreateViewBeforeTranslate();

  // Creates the 'translating' view. Caller takes ownership of the returned
  // view.
  views::View* CreateViewTranslating();

  // Creates the 'after translate' view. Caller takes ownership of the returned
  // view.
  views::View* CreateViewAfterTranslate();

  // Creates the 'error' view. Caller takes ownership of the returned view.
  views::View* CreateViewError();

  // Creates the 'advanced' view. Caller takes ownership of the returned view.
  views::View* CreateViewAdvanced();

  // Get the current always translate checkbox
  views::Checkbox* GetAlwaysTranslateCheckbox();

  // Switches the view type.
  void SwitchView(TranslateBubbleModel::ViewState view_state);

  // Switches to the error view.
  void SwitchToErrorView(translate::TranslateErrors::Type error_type);

  // Updates the advanced view.
  void UpdateAdvancedView();

  static TranslateBubbleView* translate_bubble_view_;

  views::View* before_translate_view_;
  views::View* translating_view_;
  views::View* after_translate_view_;
  views::View* error_view_;
  views::View* advanced_view_;

  std::unique_ptr<ui::SimpleComboboxModel> denial_combobox_model_;
  std::unique_ptr<LanguageComboboxModel> source_language_combobox_model_;
  std::unique_ptr<LanguageComboboxModel> target_language_combobox_model_;

  views::Combobox* denial_combobox_;
  views::Combobox* source_language_combobox_;
  views::Combobox* target_language_combobox_;

  views::Checkbox* before_always_translate_checkbox_;
  views::Checkbox* advanced_always_translate_checkbox_;

  views::LabelButton* advanced_cancel_button_;
  views::LabelButton* advanced_done_button_;

  views::MenuButton* denial_menu_button_;
  std::unique_ptr<ui::SimpleMenuModel> denial_menu_model_;
  std::unique_ptr<views::MenuRunner> denial_menu_runner_;

  std::unique_ptr<TranslateBubbleModel> model_;

  translate::TranslateErrors::Type error_type_;

  // Whether the window is an incognito window.
  const bool is_in_incognito_window_;

  bool should_always_translate_;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
