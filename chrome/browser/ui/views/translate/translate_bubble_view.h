// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ui/translate/language_combobox_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"

class Browser;
class PrefService;
class TranslateBubbleModel;

namespace content {
class WebContents;
}

namespace views {
class Checkbox;
class GridLayout;
class LabelButton;
class Link;
class View;
}

class TranslateBubbleView : public views::BubbleDelegateView,
                            public views::ButtonListener,
                            public views::ComboboxListener,
                            public views::LinkListener {
 public:
  virtual ~TranslateBubbleView();

  // Shows the Translate bubble.
  static void ShowBubble(views::View* anchor_view,
                         content::WebContents* web_contents,
                         TranslateBubbleModel::ViewState type,
                         Browser* browser);

  // If true, the Translate bubble is being shown.
  static bool IsShowing();

  // Returns the bubble view currently shown. This may return NULL.
  static TranslateBubbleView* GetCurrentBubble();

  TranslateBubbleModel* model() { return model_.get(); }

  // views::BubbleDelegateView methods.
  virtual void Init() OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::WidgetDelegate method.
  virtual void WindowClosing() OVERRIDE;

  // views::View methods.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::CombboxListener method.
  virtual void OnSelectedIndexChanged(views::Combobox* combobox) OVERRIDE;

  // views::LinkListener method.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Returns the current view state.
  TranslateBubbleModel::ViewState GetViewState() const;

 private:
  enum LinkID {
    LINK_ID_ADVANCED,
    LINK_ID_LEARN_MORE,
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
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TranslateButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, AdvancedLink);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, ShowOriginalButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TryAgainButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndCancelButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, DoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           CancelButtonReturningBeforeTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           CancelButtonReturningAfterTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, CancelButtonReturningError);

  TranslateBubbleView(views::View* anchor_view,
                      scoped_ptr<TranslateBubbleModel> model,
                      bool is_in_incognito_window,
                      Browser* browser);

  // Returns the current child view.
  views::View* GetCurrentView();

  // Handles the event when the user presses a button.
  void HandleButtonPressed(ButtonID sender_id);

  // Handles the event when the user clicks a link.
  void HandleLinkClicked(LinkID sender_id);

  // Handles the event when the user changes an index of a combobox.
  void HandleComboboxSelectedIndexChanged(ComboboxID sender_id);

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

  // Switches the view type.
  void SwitchView(TranslateBubbleModel::ViewState view_state);

  // Updates the advanced view.
  void UpdateAdvancedView();

  static TranslateBubbleView* translate_bubble_view_;

  views::View* before_translate_view_;
  views::View* translating_view_;
  views::View* after_translate_view_;
  views::View* error_view_;
  views::View* advanced_view_;

  scoped_ptr<LanguageComboboxModel> source_language_combobox_model_;
  scoped_ptr<LanguageComboboxModel> target_language_combobox_model_;

  views::Combobox* denial_combobox_;
  views::Combobox* source_language_combobox_;
  views::Combobox* target_language_combobox_;

  views::Checkbox* always_translate_checkbox_;

  scoped_ptr<TranslateBubbleModel> model_;

  // Whether the window is an incognito window.
  bool is_in_incognito_window_;

  // The browser to open the help URL into a new tab.
  Browser* browser_;

  // Whether the translation is acutually executed.
  bool translate_executed_;

  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
