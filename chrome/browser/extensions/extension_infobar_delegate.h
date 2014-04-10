// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class GURL;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
class ExtensionViewHost;
}

// The InfobarDelegate for creating and managing state for the ExtensionInfobar
// plus monitor when the extension goes away.
class ExtensionInfoBarDelegate : public InfoBarDelegate,
                                 public content::NotificationObserver {
 public:
  virtual ~ExtensionInfoBarDelegate();

  // Creates an extension infobar and delegate and adds the infobar to the
  // infobar service for |web_contents|.
  static void Create(content::WebContents* web_contents,
                     Browser* browser,
                     const extensions::Extension* extension,
                     const GURL& url,
                     int height);

  const extensions::Extension* extension() { return extension_; }
  extensions::ExtensionViewHost* extension_view_host() {
    return extension_view_host_.get();
  }
  int height() { return height_; }

  bool closing() const { return closing_; }

  // Returns the WebContents associated with the ExtensionInfoBarDelegate.
  content::WebContents* GetWebContents();

 private:
  ExtensionInfoBarDelegate(Browser* browser,
                           const extensions::Extension* extension,
                           const GURL& url,
                           content::WebContents* web_contents,
                           int height);

  // Returns an extension infobar that owns |delegate|.
  static scoped_ptr<InfoBar> CreateInfoBar(
      scoped_ptr<ExtensionInfoBarDelegate> delegate);

  // InfoBarDelegate:
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual ExtensionInfoBarDelegate* AsExtensionInfoBarDelegate() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

#if defined(TOOLKIT_VIEWS)
  Browser* browser_;  // We pass this to the ExtensionInfoBar.
#endif

  // The extension host we are showing the InfoBar for.
  // TODO(pkasting): Should this live on the InfoBar instead?
  scoped_ptr<extensions::ExtensionViewHost> extension_view_host_;

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
