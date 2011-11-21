// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "ui/gfx/native_widget_types.h"

class TabContents;
class HungRendererDialogHandler;

class HungRendererDialog : private HtmlDialogUIDelegate {
 public:
  // Shows a hung renderer dialog.
  static void ShowHungRendererDialog(TabContents* contents);

  // Hides a hung renderer dialog.
  static void HideHungRendererDialog(TabContents* contents);

 private:
  class TabContentsObserverImpl : public TabContentsObserver {
   public:
    TabContentsObserverImpl(HungRendererDialog* dialog,
                            TabContents* contents);

    // TabContentsObserver overrides:
    virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
    virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

   private:
    TabContents* contents_;  // weak
    HungRendererDialog* dialog_;  // weak

    DISALLOW_COPY_AND_ASSIGN(TabContentsObserverImpl);
  };

  friend class HungRendererDialogUITest;

  explicit HungRendererDialog(bool is_enabled);
  virtual ~HungRendererDialog();

  // Shows a hung renderer dialog that, if not enabled, won't kill processes
  // or restart hang timers.
  static void ShowHungRendererDialogInternal(TabContents* contents,
                                             bool is_enabled);

  // Shows the hung renderer dialog.
  void ShowDialog(TabContents* contents);

  // Hides the hung renderer dialog.
  void HideDialog(TabContents* contents);

  // HtmlDialogUIDelegate methods
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  // The tab contents.
  TabContents* contents_;

  // The dialog handler.
  HungRendererDialogHandler* handler_;

  // A safety switch that must be enabled to allow actual killing of processes
  // or restarting of the hang timer.  This is necessary so that tests can
  // create a disabled version of this dialog that won't kill processes or
  // restart timers when the dialog closes at the end of the test.
  bool is_enabled_;

  // The dialog window.
  gfx::NativeWindow window_;

  scoped_ptr<TabContentsObserverImpl> contents_observer_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialog);
};

// Dialog handler that handles calls from the JS WebUI code to get the details
// of the list of frozen tabs.
class HungRendererDialogHandler : public WebUIMessageHandler {
 public:
  explicit HungRendererDialogHandler(TabContents* contents);

  void CloseDialog();

  // Overridden from WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

 private:
  void RequestTabContentsList(const base::ListValue* args);

  // The tab contents.
  TabContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogHandler);
};


#endif  // CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_H_

