// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_STUB_WEB_VIEW_H_
#define CHROME_TEST_CHROMEDRIVER_STUB_WEB_VIEW_H_

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/web_view.h"

namespace base {
class ListValue;
class Value;
}

struct KeyEvent;
struct MouseEvent;
class Status;

class StubWebView : public WebView {
 public:
  explicit StubWebView(const std::string& id);
  virtual ~StubWebView();

  // Overridden from WebView:
  virtual std::string GetId() OVERRIDE;
  virtual Status Close() OVERRIDE;
  virtual Status Load(const std::string& url) OVERRIDE;
  virtual Status Reload() OVERRIDE;
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& function,
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
  virtual Status GetMainFrame(std::string* frame_id) OVERRIDE;
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() OVERRIDE;
  virtual Status CaptureScreenshot(std::string* screenshot) OVERRIDE;

 private:
  std::string id_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_STUB_WEB_VIEW_H_
