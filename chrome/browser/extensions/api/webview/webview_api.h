// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/script_executor.h"

class WebviewExecuteScriptFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.executeScript", WEBVIEW_EXECUTESCRIPT)

  WebviewExecuteScriptFunction();

 protected:
  virtual ~WebviewExecuteScriptFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:

  void OnExecuteCodeFinished(const std::string& error,
                             int32 on_page_id,
                             const GURL& on_url,
                             const ListValue& result);

};
#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
