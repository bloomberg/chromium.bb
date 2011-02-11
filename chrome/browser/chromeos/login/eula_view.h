// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {

class Checkbox;
class Label;
class Link;
class NativeButton;

}  // namespace views

class DOMView;

namespace chromeos {

class HelpAppLauncher;
class MetricsCrosSettingsProvider;

// Delegate for TabContents that will show EULA.
// Blocks context menu and other actions.
class EULATabContentsDelegate : public TabContentsDelegate {
 public:
  EULATabContentsDelegate() {}
  virtual ~EULATabContentsDelegate() {}

 protected:
  // TabContentsDelegate implementation:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type) {
    return false;
  }
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EULATabContentsDelegate);
};

class EulaView
    : public views::View,
      public views::ButtonListener,
      public views::LinkController,
      public MessageBubbleDelegate,
      public EULATabContentsDelegate {
 public:
  explicit EulaView(chromeos::ScreenObserver* observer);
  virtual ~EulaView();

  // Initialize view controls and layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

 protected:
  // views::View implementation.
  virtual void OnLocaleChanged();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController implementation.
  void LinkActivated(views::Link* source, int event_flags);

 private:
  // views::View implementation.
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
    return true; }
  virtual bool OnKeyPressed(const views::KeyEvent& e);

  // TabContentsDelegate implementation.
  virtual void NavigationStateChanged(const TabContents* contents,
                                      unsigned changed_flags);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Loads specified URL to the specified DOMView and updates specified
  // label with its title.
  void LoadEulaView(DOMView* eula_view,
                    views::Label* eula_label,
                    const GURL& eula_url);

  // Overridden from views::InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) { bubble_ = NULL; }
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }
  virtual void OnHelpLinkActivated() {}

  // Dialog controls.
  views::Label* google_eula_label_;
  DOMView* google_eula_view_;
  views::Checkbox* usage_statistics_checkbox_;
  views::Link* learn_more_link_;
  views::Label* oem_eula_label_;
  DOMView* oem_eula_view_;
  views::Link* system_security_settings_link_;
  views::NativeButton* back_button_;
  views::NativeButton* continue_button_;

  chromeos::ScreenObserver* observer_;

  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // TPM password local storage. By convention, we clear the password
  // from TPM as soon as we read it. We store it here locally until
  // EULA screen is closed.
  // TODO(glotov): Sanitize memory used to store password when
  // it's destroyed.
  std::string tpm_password_;

  DISALLOW_COPY_AND_ASSIGN(EulaView);
};

class EulaScreen : public DefaultViewScreen<EulaView> {
 public:
  explicit EulaScreen(WizardScreenDelegate* delegate)
      : DefaultViewScreen<EulaView>(delegate) {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
