// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/base_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Extension;
class ExtensionWindowController;
class GURL;
class Profile;
class TabContentsWrapper;

namespace content {
class WebContents;
}

// ShellWindow is the type of window used by platform apps. Shell windows
// have a WebContents but none of the chrome of normal browser windows.
class ShellWindow : public content::NotificationObserver,
                    public content::WebContentsDelegate,
                    public content::WebContentsObserver,
                    public ExtensionFunctionDispatcher::Delegate,
                    public BaseWindow {
 public:
  static ShellWindow* Create(Profile* profile,
                             const Extension* extension,
                             const GURL& url);

  const SessionID& session_id() const { return session_id_; }
  const ExtensionWindowController* extension_window_controller() const {
    return extension_window_controller_.get();
  }

 protected:
  // TODO(mihaip): Switch from hardcoded defaults to passing in the window
  // creation parameters to ShellWindow::Create.
  static const int kDefaultWidth = 512;
  static const int kDefaultHeight = 384;

  ShellWindow(Profile* profile,
              const Extension* extension,
              const GURL& url);
  virtual ~ShellWindow();

  const Extension* extension() const { return extension_; }
  content::WebContents* web_contents() const { return web_contents_; }

 private:
  // PlatformAppBrowserTest needs access to web_contents()
  friend class PlatformAppBrowserTest;

  // Instantiates a platform-specific ShellWindow subclass (one implementation
  // per platform). Public users of ShellWindow should use ShellWindow::Create.
  static ShellWindow* CreateImpl(Profile* profile,
                                 const Extension* extension,
                                 const GURL& url);

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::WebContentsDelegate implementation.
  virtual void CloseContents(content::WebContents* contents) OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual void WebIntentDispatch(
      content::WebContents* web_contents,
      content::WebIntentsDispatcher* intents_dispatcher) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate implementation.
  virtual ExtensionWindowController* GetExtensionWindowController() const
      OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  Profile* profile_;  // weak pointer - owned by ProfileManager.
  const Extension* extension_;  // weak pointer - owned by ExtensionService.

  const SessionID session_id_;
  scoped_ptr<TabContentsWrapper> contents_wrapper_;
  // web_contents_ is owned by contents_wrapper_.
  content::WebContents* web_contents_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<ExtensionWindowController> extension_window_controller_;
  ExtensionFunctionDispatcher extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
