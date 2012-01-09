// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class Extension;
class ExtensionHost;
class Profile;

namespace content {
class WebContents;
}

class ShellWindow : public content::NotificationObserver {
 public:
  content::WebContents* web_contents() const { return host_->host_contents(); }

  static ShellWindow* Create(Profile* profile,
                             const Extension* extension,
                             const GURL& url);

  // Closes the displayed window and invokes the destructor.
  virtual void Close() = 0;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  explicit ShellWindow(ExtensionHost* host_);
  virtual ~ShellWindow();

  // Instantiates a platform-specific ShellWindow subclass (one implementation
  // per platform). Public users of ShellWindow should use ShellWindow::Create.
  static ShellWindow* CreateShellWindow(ExtensionHost* host);

  scoped_ptr<ExtensionHost> host_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
