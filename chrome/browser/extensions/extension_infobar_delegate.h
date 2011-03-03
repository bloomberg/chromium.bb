// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
#pragma once

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

class Browser;
class Extension;
class ExtensionHost;
class TabContents;

// The InfobarDelegate for creating and managing state for the ExtensionInfobar
// plus monitor when the extension goes away.
class ExtensionInfoBarDelegate : public InfoBarDelegate,
                                 public NotificationObserver {
 public:
  // The observer for when the delegate dies.
  class DelegateObserver {
   public:
    virtual void OnDelegateDeleted() = 0;

   protected:
    virtual ~DelegateObserver() {}
  };

  ExtensionInfoBarDelegate(Browser* browser,
                           TabContents* contents,
                           const Extension* extension,
                           const GURL& url);

  const Extension* extension() { return extension_; }
  ExtensionHost* extension_host() { return extension_host_.get(); }

  void set_observer(DelegateObserver* observer) { observer_ = observer; }

  bool closing() const { return closing_; }

 private:
  virtual ~ExtensionInfoBarDelegate();

  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;
  virtual void InfoBarDismissed();
  virtual void InfoBarClosed();
  virtual Type GetInfoBarType() const;
  virtual ExtensionInfoBarDelegate* AsExtensionInfoBarDelegate();

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The extension host we are showing the InfoBar for. The delegate needs to
  // own this since the InfoBar gets deleted and recreated when you switch tabs
  // and come back (and we don't want the user's interaction with the InfoBar to
  // get lost at that point).
  scoped_ptr<ExtensionHost> extension_host_;

  // The observer monitoring when the delegate dies.
  DelegateObserver* observer_;

  const Extension* extension_;
  TabContents* tab_contents_;
  NotificationRegistrar registrar_;

  // Whether we are currently animating to close. This is used to ignore
  // ExtensionView::PreferredSizeChanged notifications.
  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
