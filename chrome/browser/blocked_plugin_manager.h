// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLOCKED_PLUGIN_MANAGER_H_
#define CHROME_BROWSER_BLOCKED_PLUGIN_MANAGER_H_

#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"

class TabContents;

class BlockedPluginManager : public RenderViewHostDelegate::BlockedPlugin,
                             public ConfirmInfoBarDelegate {
 public:
  explicit BlockedPluginManager(TabContents* tab_contents);

  virtual void OnNonSandboxedPluginBlocked(const std::string& plugin,
                                           const string16& name);
  virtual void OnBlockedPluginLoaded();

  // ConfirmInfoBarDelegate methods
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual string16 GetMessageText() const;
  virtual string16 GetLinkText();
  virtual SkBitmap* GetIcon() const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void InfoBarDismissed();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 private:
  // Owns us.
  TabContents* tab_contents_;
  string16 name_;
  std::string plugin_;
};

#endif  // CHROME_BROWSER_BLOCKED_PLUGIN_MANAGER_H_
