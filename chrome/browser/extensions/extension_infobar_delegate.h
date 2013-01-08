// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class GURL;
class InfoBarService;

namespace extensions {
class Extension;
class ExtensionHost;
}

// The InfobarDelegate for creating and managing state for the ExtensionInfobar
// plus monitor when the extension goes away.
class ExtensionInfoBarDelegate : public InfoBarDelegate,
                                 public content::NotificationObserver {
 public:
  // The observer for when the delegate dies.
  class DelegateObserver {
   public:
    virtual void OnDelegateDeleted() = 0;

   protected:
    virtual ~DelegateObserver() {}
  };

  virtual ~ExtensionInfoBarDelegate();

  // Creates an extension delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     Browser* browser,
                     const extensions::Extension* extension,
                     const GURL& url,
                     int height);

  const extensions::Extension* extension() { return extension_; }
  extensions::ExtensionHost* extension_host() { return extension_host_.get(); }
  int height() { return height_; }

  void set_observer(DelegateObserver* observer) { observer_ = observer; }

  bool closing() const { return closing_; }

 private:
  ExtensionInfoBarDelegate(Browser* browser,
                           InfoBarService* infobar_service,
                           const extensions::Extension* extension,
                           const GURL& url,
                           int height);

  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual ExtensionInfoBarDelegate* AsExtensionInfoBarDelegate() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Browser* browser_;

  // The extension host we are showing the InfoBar for. The delegate needs to
  // own this since the InfoBar gets deleted and recreated when you switch tabs
  // and come back (and we don't want the user's interaction with the InfoBar to
  // get lost at that point).
  scoped_ptr<extensions::ExtensionHost> extension_host_;

  // The observer monitoring when the delegate dies.
  DelegateObserver* observer_;

  const extensions::Extension* extension_;
  content::NotificationRegistrar registrar_;

  // The requested height of the infobar (in pixels).
  int height_;

  // Whether we are currently animating to close. This is used to ignore
  // ExtensionView::PreferredSizeChanged notifications.
  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
