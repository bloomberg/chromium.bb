// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_

#include "chrome/browser/extensions/api/capture_web_contents_function.h"
#include "chrome/browser/extensions/api/execute_code_function.h"
#include "chrome/browser/guestview/webview/webview_find_helper.h"
#include "chrome/browser/guestview/webview/webview_guest.h"

// WARNING: Webview could be loaded in an unblessed context, thus any new
// APIs must extend WebviewExtensionFunction/WebviewExecuteCodeFunction which
// do a process ID check to prevent abuse by normal renderer processes.
// TODO(guohui): refactor WebviewExecuteCodeFunction to also extend
// WebviewExtensionFunction.
namespace extensions {

// An abstract base class for async webview APIs. It does a process ID check
// in RunImpl, and then calls RunImplSafe which must be overriden by all
// subclasses.
class WebviewExtensionFunction : public AsyncExtensionFunction {
 public:
   WebviewExtensionFunction() {}

 protected:
  virtual ~WebviewExtensionFunction() {}

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE FINAL;

 private:
  virtual bool RunImplSafe(WebViewGuest* guest) = 0;
};

class WebviewContextMenusCreateFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.contextMenusCreate",
                             WEBVIEW_CONTEXTMENUSCREATE);
  WebviewContextMenusCreateFunction() {}

 protected:
  virtual ~WebviewContextMenusCreateFunction() {}

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewContextMenusCreateFunction);
};

class WebviewContextMenusUpdateFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.contextMenusUpdate",
                             WEBVIEW_CONTEXTMENUSUPDATE);
  WebviewContextMenusUpdateFunction() {}

 protected:
  virtual ~WebviewContextMenusUpdateFunction() {}

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewContextMenusUpdateFunction);
};

class WebviewContextMenusRemoveFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.contextMenusRemove",
                             WEBVIEW_CONTEXTMENUSREMOVE);
  WebviewContextMenusRemoveFunction() {}

 protected:
  virtual ~WebviewContextMenusRemoveFunction() {}

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewContextMenusRemoveFunction);
};

class WebviewContextMenusRemoveAllFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.contextMenusRemoveAll",
                             WEBVIEW_CONTEXTMENUSREMOVEALL);
  WebviewContextMenusRemoveAllFunction() {}

 protected:
  virtual ~WebviewContextMenusRemoveAllFunction() {}

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewContextMenusRemoveAllFunction);
};

class WebviewClearDataFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.clearData", WEBVIEW_CLEARDATA);

  WebviewClearDataFunction();

 protected:
  virtual ~WebviewClearDataFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

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
  // Guarded by a process ID check.
  virtual extensions::ScriptExecutor* GetScriptExecutor() OVERRIDE FINAL;
  virtual bool IsWebView() const OVERRIDE;
  virtual const GURL& GetWebViewSrc() const OVERRIDE;

 private:
  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  extensions::ExtensionResource resource_;

  int guest_instance_id_;

  GURL guest_src_;

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

class WebviewCaptureVisibleRegionFunction
    : public extensions::CaptureWebContentsFunction {
  DECLARE_EXTENSION_FUNCTION("webview.captureVisibleRegion",
                             WEBVIEW_CAPTUREVISIBLEREGION);

  WebviewCaptureVisibleRegionFunction();

 protected:
  virtual ~WebviewCaptureVisibleRegionFunction();

 private:
  // extensions::CaptureWebContentsFunction implementation.
  virtual bool IsScreenshotEnabled() OVERRIDE;
  virtual content::WebContents* GetWebContentsForID(int id) OVERRIDE;
  virtual void OnCaptureFailure(FailureReason reason) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewCaptureVisibleRegionFunction);
};

class WebviewSetZoomFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.setZoom", WEBVIEW_SETZOOM);

  WebviewSetZoomFunction();

 protected:
  virtual ~WebviewSetZoomFunction();

 private:
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewSetZoomFunction);
};

class WebviewGetZoomFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.getZoom", WEBVIEW_GETZOOM);

  WebviewGetZoomFunction();

 protected:
  virtual ~WebviewGetZoomFunction();

 private:
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewGetZoomFunction);
};

class WebviewFindFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.find", WEBVIEW_FIND);

  WebviewFindFunction();

  // Exposes SendResponse() for use by WebviewFindHelper.
  using WebviewExtensionFunction::SendResponse;

 protected:
  virtual ~WebviewFindFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewFindFunction);
};

class WebviewStopFindingFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.stopFinding", WEBVIEW_STOPFINDING);

  WebviewStopFindingFunction();

 protected:
  virtual ~WebviewStopFindingFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewStopFindingFunction);
};

class WebviewGoFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.go", WEBVIEW_GO);

  WebviewGoFunction();

 protected:
  virtual ~WebviewGoFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewGoFunction);
};

class WebviewReloadFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.reload", WEBVIEW_RELOAD);

  WebviewReloadFunction();

 protected:
  virtual ~WebviewReloadFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewReloadFunction);
};

class WebviewSetPermissionFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.setPermission", WEBVIEW_SETPERMISSION);

  WebviewSetPermissionFunction();

 protected:
  virtual ~WebviewSetPermissionFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewSetPermissionFunction);
};

class WebviewOverrideUserAgentFunction: public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.overrideUserAgent",
                             WEBVIEW_OVERRIDEUSERAGENT);

  WebviewOverrideUserAgentFunction();

 protected:
  virtual ~WebviewOverrideUserAgentFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewOverrideUserAgentFunction);
};

class WebviewStopFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.stop", WEBVIEW_STOP);

  WebviewStopFunction();

 protected:
  virtual ~WebviewStopFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewStopFunction);
};

class WebviewTerminateFunction : public WebviewExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webview.terminate", WEBVIEW_TERMINATE);

  WebviewTerminateFunction();

 protected:
  virtual ~WebviewTerminateFunction();

 private:
  // WebviewExtensionFunction implementation.
  virtual bool RunImplSafe(WebViewGuest* guest) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewTerminateFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBVIEW_WEBVIEW_API_H_
