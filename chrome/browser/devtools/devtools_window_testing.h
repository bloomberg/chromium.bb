// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_TESTING_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_TESTING_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "ui/gfx/geometry/rect.h"

class Browser;
class Profile;

namespace content {
class DevToolsAgentHost;
class MessageLoopRunner;
class WebContents;
}

class DevToolsWindowTesting {
 public:
  virtual ~DevToolsWindowTesting();

  // The following methods block until DevToolsWindow is completely loaded.
  static DevToolsWindow* OpenDevToolsWindowSync(
      content::WebContents* inspected_web_contents,
      bool is_docked);
  static DevToolsWindow* OpenDevToolsWindowSync(
      Browser* browser, bool is_docked);
  static DevToolsWindow* OpenDevToolsWindowForWorkerSync(
      Profile* profile, content::DevToolsAgentHost* worker_agent);

  // Closes the window like it was user-initiated.
  static void CloseDevToolsWindow(DevToolsWindow* window);
  // Blocks until window is closed.
  static void CloseDevToolsWindowSync(DevToolsWindow* window);

  static DevToolsWindowTesting* Get(DevToolsWindow* window);

  Browser* browser();
  content::WebContents* main_web_contents();
  content::WebContents* toolbox_web_contents();
  void SetInspectedPageBounds(const gfx::Rect& bounds);
  void SetCloseCallback(const base::Closure& closure);

 private:
  friend class DevToolsWindow;
  friend class DevToolsWindowCreationObserver;

  explicit DevToolsWindowTesting(DevToolsWindow* window);
  static void WaitForDevToolsWindowLoad(DevToolsWindow* window);
  static void WindowClosed(DevToolsWindow* window);
  static DevToolsWindowTesting* Find(DevToolsWindow* window);

  DevToolsWindow* devtools_window_;
  base::Closure close_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowTesting);
};

class DevToolsWindowCreationObserver {
 public:
  DevToolsWindowCreationObserver();
  ~DevToolsWindowCreationObserver();

  DevToolsWindow* devtools_window() { return devtools_window_; }
  void Wait();
  void WaitForLoad();

 private:
  friend class DevToolsWindow;

  void DevToolsWindowCreated(DevToolsWindow* devtools_window);

  DevToolsWindow* devtools_window_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowCreationObserver);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_TESTING_H_
