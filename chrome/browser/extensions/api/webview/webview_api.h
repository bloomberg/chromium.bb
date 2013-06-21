// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_

#include "chrome/browser/extensions/api/execute_code_function.h"

class WebviewExecuteCodeFunction : public extensions::ExecuteCodeFunction {
 public:
  WebviewExecuteCodeFunction();

 protected:
  virtual ~WebviewExecuteCodeFunction();

  // Initialize |details_| if it hasn't already been.
  virtual bool Init() OVERRIDE;
  virtual bool ShouldInsertCSS() const OVERRIDE;
  virtual bool CanExecuteScriptOnPage() OVERRIDE;
  virtual extensions::ScriptExecutor* GetScriptExecutor() OVERRIDE;
  virtual bool IsWebView() const OVERRIDE;

 private:
  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  extensions::ExtensionResource resource_;

  int guest_instance_id_;
};

class WebviewExecuteScriptFunction : public WebviewExecuteCodeFunction {
 protected:
  virtual ~WebviewExecuteScriptFunction() {}

  virtual void OnExecuteCodeFinished(const std::string& error,
                                     int32 on_page_id,
                                     const GURL& on_url,
                                     const base::ListValue& result) OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("webview.executeScript", WEBVIEW_EXECUTESCRIPT)
};

class WebviewInsertCSSFunction : public WebviewExecuteCodeFunction {
 protected:
  virtual ~WebviewInsertCSSFunction() {}

  virtual bool ShouldInsertCSS() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("webview.insertCSS", WEBVIEW_INSERTCSS)
};
#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
