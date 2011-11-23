// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COLLECTED_COOKIES_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_COLLECTED_COOKIES_UI_DELEGATE_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/cookies_tree_model_adapter.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class TabContents;
class TabContentsWrapper;

namespace gfx {
class Size;
}

class CollectedCookiesUIDelegate : public HtmlDialogUIDelegate,
                                          WebUIMessageHandler,
                                          content::NotificationObserver {
 public:
  virtual ~CollectedCookiesUIDelegate();

  // static factory method that shows CollectedCookiesUI for |tab_contents|.
  static void Show(TabContentsWrapper* wrapper);

  // HtmlDialogUIDelegate implementation:
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog)
      OVERRIDE {}
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  explicit CollectedCookiesUIDelegate(TabContentsWrapper* wrapper);

  // Closes the dialog from javascript.
  void CloseDialog();

  // Sets infobar label text.
  void SetInfobarLabel(const std::string& text);

  // Add content exception.
  void AddContentException(CookieTreeOriginNode* origin_node,
                           ContentSetting setting);

  // Notification Observer implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // JS callback to bind cookies tree models with JS trees.
  void BindCookiesTreeModel(const base::ListValue* args);

  // JS callback to block/allow cookies from given site.
  void Block(const base::ListValue* args);
  void Allow(const base::ListValue* args);
  void AllowThisSession(const base::ListValue* args);

  content::NotificationRegistrar registrar_;
  TabContentsWrapper* wrapper_;
  bool closed_;

  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;

  CookiesTreeModelAdapter allowed_cookies_adapter_;
  CookiesTreeModelAdapter blocked_cookies_adapter_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesUIDelegate);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COLLECTED_COOKIES_UI_DELEGATE_H_
