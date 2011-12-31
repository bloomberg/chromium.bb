// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TEST_HTML_DIALOG_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_TEST_HTML_DIALOG_UI_DELEGATE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/size.h"

namespace test {

class TestHtmlDialogUIDelegate : public HtmlDialogUIDelegate {
 public:
  explicit TestHtmlDialogUIDelegate(const GURL& url);
  virtual ~TestHtmlDialogUIDelegate();

  void set_size(int width, int height) {
    size_.SetSize(width, height);
  }

  // HTMLDialogUIDelegate implementation:
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

 protected:
  const GURL url_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TestHtmlDialogUIDelegate);
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_WEBUI_TEST_HTML_DIALOG_UI_DELEGATE_H_
