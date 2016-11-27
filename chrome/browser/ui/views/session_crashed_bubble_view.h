// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/session_crashed_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class Checkbox;
class Widget;
}

namespace content {
class WebContents;
}

class Browser;

// It creates a session restore request bubble when the previous session has
// crashed. It also presents an option to enable metrics reporting, if it not
// enabled already.
class SessionCrashedBubbleView : public SessionCrashedBubble,
                                 public views::BubbleDialogDelegateView,
                                 public views::StyledLabelListener,
                                 public content::WebContentsObserver,
                                 public content::NotificationObserver,
                                 public TabStripModelObserver {
 public:
  // A helper class that listens to browser removal event.
  class BrowserRemovalObserver;

  // Creates and shows the session crashed bubble, with |uma_opted_in_already|
  // indicating whether the user has already opted-in to UMA. It will be called
  // by Show. It takes ownership of |browser_observer|.
  static void ShowForReal(
      std::unique_ptr<BrowserRemovalObserver> browser_observer,
      bool uma_opted_in_already);

 private:
  SessionCrashedBubbleView(views::View* anchor_view,
                           Browser* browser,
                           content::WebContents* web_contents,
                           bool offer_uma_optin);
  ~SessionCrashedBubbleView() override;

  // WidgetDelegateView methods.
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  views::View* CreateFootnoteView() override;
  bool Accept() override;
  bool Close() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // views::BubbleDialogDelegateView methods.
  void Init() override;

  // views::StyledLabelListener methods.
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // content::WebContentsObserver methods.
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::ReloadType reload_type) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WasShown() override;
  void WasHidden() override;

  // content::NotificationObserver methods.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // TabStripModelObserver methods.
  // When the tab with current bubble is being dragged and dropped to a new
  // window or to another window, the bubble will be dismissed as if the user
  // chose not to restore the previous session.
  void TabDetachedAt(content::WebContents* contents, int index) override;

  // Restore previous session after user selects so.
  void RestorePreviousSession();

  // Close and destroy the bubble.
  void CloseBubble();

  content::NotificationRegistrar registrar_;

  // Used for opening the question mark link as well as access the tab strip.
  Browser* browser_;

  // The web content associated with current bubble.
  content::WebContents* web_contents_;

  // Checkbox for the user to opt-in to UMA reporting.
  views::Checkbox* uma_option_;

  // Whether or not the UMA opt-in option should be shown.
  bool offer_uma_optin_;

  // Whether or not a navigation has started on current tab.
  bool started_navigation_;

  // Whether or not the user chose to restore previous session. It is used to
  // collect bubble usage stats.
  bool restored_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
