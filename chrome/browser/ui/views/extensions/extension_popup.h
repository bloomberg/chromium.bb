// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "url/gurl.h"


class Browser;
namespace views {
class Widget;
}

namespace content {
class DevToolsAgentHost;
}

namespace extensions {
class ExtensionViewHost;
}

class ExtensionPopup : public views::BubbleDelegateView,
                       public ExtensionViewViews::Container,
                       public content::NotificationObserver,
                       public TabStripModelObserver {
 public:
  enum ShowAction {
    SHOW,
    SHOW_AND_INSPECT,
  };

  ~ExtensionPopup() override;

  // Create and show a popup with |url| positioned adjacent to |anchor_view|.
  // |browser| is the browser to which the pop-up will be attached.  NULL is a
  // valid parameter for pop-ups not associated with a browser.
  // The positioning of the pop-up is determined by |arrow| according to the
  // following logic:  The popup is anchored so that the corner indicated by the
  // value of |arrow| remains fixed during popup resizes.  If |arrow| is
  // BOTTOM_*, then the popup 'pops up', otherwise the popup 'drops down'.
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static ExtensionPopup* ShowPopup(const GURL& url,
                                   Browser* browser,
                                   views::View* anchor_view,
                                   views::BubbleBorder::Arrow arrow,
                                   ShowAction show_action);

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionViewViews::Container overrides.
  void OnExtensionSizeChanged(ExtensionViewViews* view) override;

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // views::WidgetObserver overrides.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // TabStripModelObserver overrides.
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;

  // The min/max height of popups.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 protected:
  ExtensionPopup(extensions::ExtensionViewHost* host,
                 views::View* anchor_view,
                 views::BubbleBorder::Arrow arrow,
                 ShowAction show_action);

  // Called on anchor window activation (ie. user clicked the browser window).
  void OnAnchorWindowActivation();

 private:
  static ExtensionPopup* Create(extensions::ExtensionViewHost* host,
                                views::View* anchor_view,
                                views::BubbleBorder::Arrow arrow,
                                ShowAction show_action);

  // Show the bubble, focus on its content, and register listeners.
  void ShowBubble();

  void OnDevToolsStateChanged(content::DevToolsAgentHost*, bool attached);

  // The contained host for the view.
  scoped_ptr<extensions::ExtensionViewHost> host_;

  // Flag used to indicate if the pop-up should open a devtools window once
  // it is shown inspecting it.
  bool inspect_with_devtools_;

  content::NotificationRegistrar registrar_;

  base::Callback<void(content::DevToolsAgentHost*, bool)> devtools_callback_;

  bool widget_initialized_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
