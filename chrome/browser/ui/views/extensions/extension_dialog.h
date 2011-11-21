// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "views/widget/widget_delegate.h"

class Browser;
class ExtensionDialogObserver;
class ExtensionHost;
class GURL;
class TabContents;

namespace views {
class View;
class Widget;
}

// Modal dialog containing contents provided by an extension.
// Dialog is automatically centered in the browser window and has fixed size.
// For example, used by the Chrome OS file browser.
class ExtensionDialog : public views::WidgetDelegate,
                        public content::NotificationObserver,
                        public base::RefCounted<ExtensionDialog> {
 public:
  virtual ~ExtensionDialog();

  // Create and show a dialog with |url| centered over the browser window.
  // |browser| is the browser to which the pop-up will be attached.
  // |tab_contents| is the tab that spawned the dialog.
  // |width| and |height| are the size of the dialog in pixels.
  static ExtensionDialog* Show(const GURL& url, Browser* browser,
                               TabContents* tab_contents,
                               int width,
                               int height,
                               const string16& title,
                               ExtensionDialogObserver* observer);

  // Notifies the dialog that the observer has been destroyed and should not
  // be sent notifications.
  void ObserverDestroyed();

  // Closes the ExtensionDialog.
  void Close();

  // Sets the window title.
  void set_title(const string16& title) { window_title_ = title; }

  ExtensionHost* host() const { return extension_host_.get(); }

  // views::WidgetDelegate overrides.
  virtual bool CanResize() const OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Use Show() to create instances.
  ExtensionDialog(ExtensionHost* host, ExtensionDialogObserver* observer);

  static ExtensionHost* CreateExtensionHost(const GURL& url,
                                            Browser* browser);

  void InitWindow(Browser* browser, int width, int height);

  // Window that holds the extension host view.
  views::Widget* window_;

  // Window Title
  string16 window_title_;

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  content::NotificationRegistrar registrar_;

  // The observer of this popup.
  ExtensionDialogObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
