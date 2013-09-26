// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_

#include "chrome/browser/extensions/api/execute_code_function.h"

namespace extensions {

class WebviewClearDataFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.clearData", WEBVIEW_CLEARDATA);

  WebviewClearDataFunction();

 protected:
  virtual ~WebviewClearDataFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  uint32 GetRemovalMask();
  void ClearDataDone();

  // Removal start time.
  base::Time remove_since_;
  // Removal mask, corresponds to StoragePartition::RemoveDataMask enum.
  uint32 remove_mask_;
  // Tracks any data related or parse errors.
  bool bad_message_;

  DISALLOW_COPY_AND_ASSIGN(WebviewClearDataFunction);
};

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

  DISALLOW_COPY_AND_ASSIGN(WebviewExecuteCodeFunction);
};

class WebviewExecuteScriptFunction : public WebviewExecuteCodeFunction {
 public:
  WebviewExecuteScriptFunction();

 protected:
  virtual ~WebviewExecuteScriptFunction() {}

  virtual void OnExecuteCodeFinished(const std::string& error,
                                     int32 on_page_id,
                                     const GURL& on_url,
                                     const base::ListValue& result) OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("webview.executeScript", WEBVIEW_EXECUTESCRIPT)

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewExecuteScriptFunction);
};

class WebviewInsertCSSFunction : public WebviewExecuteCodeFunction {
 public:
  WebviewInsertCSSFunction();

 protected:
  virtual ~WebviewInsertCSSFunction() {}

  virtual bool ShouldInsertCSS() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION("webview.insertCSS", WEBVIEW_INSERTCSS)

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewInsertCSSFunction);
};

class WebviewGoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.go", WEBVIEW_GO);

  WebviewGoFunction();

 protected:
  virtual ~WebviewGoFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewGoFunction);
};

class WebviewReloadFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.reload", WEBVIEW_RELOAD);

  WebviewReloadFunction();

 protected:
  virtual ~WebviewReloadFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewReloadFunction);
};

class WebviewSetPermissionFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.setPermission", WEBVIEW_SETPERMISSION);

  WebviewSetPermissionFunction();

 protected:
  virtual ~WebviewSetPermissionFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewSetPermissionFunction);
};

class WebviewOverrideUserAgentFunction: public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.overrideUserAgent",
                             WEBVIEW_OVERRIDEUSERAGENT);

  WebviewOverrideUserAgentFunction();

 protected:
  virtual ~WebviewOverrideUserAgentFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewOverrideUserAgentFunction);
};

class WebviewStopFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.stop", WEBVIEW_STOP);

  WebviewStopFunction();

 protected:
  virtual ~WebviewStopFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewStopFunction);
};

class WebviewTerminateFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.terminate", WEBVIEW_TERMINATE);

  WebviewTerminateFunction();

 protected:
  virtual ~WebviewTerminateFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewTerminateFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
