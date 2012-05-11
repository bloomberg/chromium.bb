// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIMPLE_WEB_VIEW_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIMPLE_WEB_VIEW_DIALOG_H_
#pragma once

#include <string>
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "googleurl/src/gurl.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_delegate.h"

class Profile;
class ReloadButton;
class ToolbarModel;

namespace views {
class WebView;
class Widget;
}

namespace chromeos {

class StubBubbleModelDelegate;

// View class which shows the light version of the toolbar and the tab contents.
// Light version of the toolbar includes back, forward buttons and location
// bar. Location bar is shown in read only mode, because this view is designed
// to be used for sign in to captive portal on login screen (when Browser
// isn't running).
class SimpleWebViewDialog : public views::ButtonListener,
                            public views::WidgetDelegateView,
                            public LocationBarView::Delegate,
                            public ToolbarModelDelegate,
                            public CommandUpdater::CommandUpdaterDelegate,
                            public content::WebContentsDelegate {
 public:
  explicit SimpleWebViewDialog(Profile* profile);
  virtual ~SimpleWebViewDialog();

  // Starts loading.
  void StartLoad(const GURL& gurl);

  // Inits view. Should be attached to a Widget before call.
  void Init();

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  // Implements views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Implements content::WebContentsDelegate:
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;

  // Implements LocationBarView::Delegate:
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual TabContentsWrapper* GetTabContentsWrapper() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual views::Widget* CreateViewsBubble(
      views::BubbleDelegateView* bubble_delegate) OVERRIDE;
  virtual PageActionImageView* CreatePageActionImageView(
      LocationBarView* owner,
      ExtensionAction* action) OVERRIDE;
  virtual ContentSettingBubbleModelDelegate*
  GetContentSettingBubbleModelDelegate() OVERRIDE;
  virtual void ShowPageInfo(content::WebContents* web_contents,
                            const GURL& url,
                            const content::SSLStatus& ssl,
                            bool show_history) OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;

  // Implements ToolbarModelDelegate:
  virtual content::WebContents* GetActiveWebContents() const OVERRIDE;

  // Implements CommandUpdaterDelegate:
  virtual void ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition) OVERRIDE;

 private:
  void LoadImages();
  void UpdateButtons();
  void UpdateReload(bool is_loading, bool force);

  Profile* profile_;
  scoped_ptr<ToolbarModel> toolbar_model_;
  scoped_ptr<CommandUpdater> command_updater_;

  // Controls
  views::ImageButton* back_;
  views::ImageButton* forward_;
  ReloadButton* reload_;
  LocationBarView* location_bar_;
  views::WebView* web_view_;

  // Contains |web_view_| while it isn't owned by the view.
  scoped_ptr<views::WebView> web_view_container_;

  scoped_ptr<StubBubbleModelDelegate> bubble_model_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWebViewDialog);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIMPLE_WEB_VIEW_DIALOG_H_
