// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_SIMPLE_WEB_VIEW_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_SIMPLE_WEB_VIEW_DIALOG_H_

#include <string>
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

class CommandUpdater;
class Profile;
class ReloadButton;
class ToolbarModel;

namespace views {
class WebView;
class Widget;
}

namespace chromeos {

class StubBubbleModelDelegate;

// View class which shows the light version of the toolbar and the web contents.
// Light version of the toolbar includes back, forward buttons and location
// bar. Location bar is shown in read only mode, because this view is designed
// to be used for sign in to captive portal on login screen (when Browser
// isn't running).
class SimpleWebViewDialog : public views::ButtonListener,
                            public views::WidgetDelegateView,
                            public LocationBarView::Delegate,
                            public ToolbarModelDelegate,
                            public CommandUpdaterDelegate,
                            public content::PageNavigator,
                            public content::WebContentsDelegate {
 public:
  explicit SimpleWebViewDialog(Profile* profile);
  virtual ~SimpleWebViewDialog();

  // Starts loading.
  void StartLoad(const GURL& gurl);

  // Inits view. Should be attached to a Widget before call.
  void Init();

  // Overridden from views::View:
  virtual void Layout() override;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() override;
  virtual views::View* GetInitiallyFocusedView() override;

  // Implements views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) override;

  // Implements content::PageNavigator:
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) override;

  // Implements content::WebContentsDelegate:
  virtual void NavigationStateChanged(
      const content::WebContents* source,
      content::InvalidateTypes changed_flags) override;
  virtual void LoadingStateChanged(content::WebContents* source,
                                   bool to_different_document) override;

  // Implements LocationBarView::Delegate:
  virtual content::WebContents* GetWebContents() override;
  virtual ToolbarModel* GetToolbarModel() override;
  virtual const ToolbarModel* GetToolbarModel() const override;
  virtual InstantController* GetInstant() override;
  virtual views::Widget* CreateViewsBubble(
      views::BubbleDelegateView* bubble_delegate) override;
  virtual PageActionImageView* CreatePageActionImageView(
      LocationBarView* owner,
      ExtensionAction* action) override;
  virtual ContentSettingBubbleModelDelegate*
  GetContentSettingBubbleModelDelegate() override;
  virtual void ShowWebsiteSettings(content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) override;

  // Implements ToolbarModelDelegate:
  virtual content::WebContents* GetActiveWebContents() const override;
  virtual bool InTabbedBrowser() const override;

  // Implements CommandUpdaterDelegate:
  virtual void ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition) override;

 private:
  friend class SimpleWebViewDialogTest;

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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_SIMPLE_WEB_VIEW_DIALOG_H_
