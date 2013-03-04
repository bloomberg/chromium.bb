// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_WEB_VIEW_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_WEB_VIEW_IMPL_H_

#include <list>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/web_view.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

class DevToolsClient;
class DomTracker;
class FrameTracker;
struct KeyEvent;
struct MouseEvent;
class NavigationTracker;
class Status;
class WebViewDelegate;

class WebViewImpl : public WebView {
 public:
  typedef base::Callback<Status()> CloserFunc;
  // Takes ownership of |client|.
  WebViewImpl(const std::string& id,
              DevToolsClient* client,
              WebViewDelegate* delegate,
              const CloserFunc& closer_func);
  virtual ~WebViewImpl();

  // Overridden from WebView:
  virtual std::string GetId() OVERRIDE;
  virtual Status Close() OVERRIDE;
  virtual Status Load(const std::string& url) OVERRIDE;
  virtual Status Reload() OVERRIDE;
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& expression,
                                scoped_ptr<base::Value>* result) OVERRIDE;
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE;
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::ListValue& args,
                                    std::string* out_frame) OVERRIDE;
  virtual Status DispatchMouseEvents(
      const std::list<MouseEvent>& events) OVERRIDE;
  virtual Status DispatchKeyEvents(const std::list<KeyEvent>& events) OVERRIDE;
  virtual Status WaitForPendingNavigations(
      const std::string& frame_id) OVERRIDE;
  virtual Status GetMainFrame(std::string* out_frame) OVERRIDE;
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() OVERRIDE;
  virtual Status CaptureScreenshot(std::string* screenshot) OVERRIDE;

 private:
  std::string id_;
  scoped_ptr<DomTracker> dom_tracker_;
  scoped_ptr<FrameTracker> frame_tracker_;
  scoped_ptr<NavigationTracker> navigation_tracker_;
  scoped_ptr<JavaScriptDialogManager> dialog_manager_;
  scoped_ptr<DevToolsClient> client_;
  WebViewDelegate* delegate_;
  CloserFunc closer_func_;
};

namespace internal {

enum EvaluateScriptReturnType {
  ReturnByValue,
  ReturnByObject
};
Status EvaluateScript(DevToolsClient* client,
                      int context_id,
                      const std::string& expression,
                      EvaluateScriptReturnType return_type,
                      scoped_ptr<base::DictionaryValue>* result);
Status EvaluateScriptAndGetObject(DevToolsClient* client,
                                  int context_id,
                                  const std::string& expression,
                                  std::string* object_id);
Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 int context_id,
                                 const std::string& expression,
                                 scoped_ptr<base::Value>* result);
Status ParseCallFunctionResult(const base::Value& temp_result,
                               scoped_ptr<base::Value>* result);
Status GetNodeIdFromFunction(DevToolsClient* client,
                             int context_id,
                             const std::string& function,
                             const base::ListValue& args,
                             int* node_id);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_WEB_VIEW_IMPL_H_
