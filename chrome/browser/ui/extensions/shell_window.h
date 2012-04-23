// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/base_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Extension;
class ExtensionHost;
class ExtensionWindowController;
class GURL;
class Profile;

namespace content {
class WebContents;
}

class ShellWindow : public content::NotificationObserver,
                    public BaseWindow {
 public:
  // TODO(mihaip): Switch from hardcoded defaults to passing in the window
  // creation parameters to ShellWindow::Create.
  static const int kDefaultWidth = 512;
  static const int kDefaultHeight = 384;

  content::WebContents* web_contents() const { return host_->host_contents(); }
  const SessionID& session_id() const { return session_id_; }
  const ExtensionWindowController* extension_window_controller() const {
    return extension_window_controller_.get();
  }

  static ShellWindow* Create(Profile* profile,
                             const Extension* extension,
                             const GURL& url);

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

  const SessionID session_id_;
  scoped_ptr<ExtensionHost> host_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<ExtensionWindowController> extension_window_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWindow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
