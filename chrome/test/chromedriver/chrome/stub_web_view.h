// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STUB_WEB_VIEW_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STUB_WEB_VIEW_H_

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

class StubWebView : public WebView {
 public:
  explicit StubWebView(const std::string& id);
  virtual ~StubWebView();

  // Overridden from WebView:
  virtual std::string GetId() override;
  virtual bool WasCrashed() override;
  virtual Status ConnectIfNecessary() override;
  virtual Status HandleReceivedEvents() override;
  virtual Status Load(const std::string& url) override;
  virtual Status Reload() override;
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& function,
                                scoped_ptr<base::Value>* result) override;
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) override;
  virtual Status CallAsyncFunction(const std::string& frame,
                                   const std::string& function,
                                   const base::ListValue& args,
                                   const base::TimeDelta& timeout,
                                   scoped_ptr<base::Value>* result) override;
  virtual Status CallUserAsyncFunction(
      const std::string& frame,
      const std::string& function,
      const base::ListValue& args,
      const base::TimeDelta& timeout,
      scoped_ptr<base::Value>* result) override;
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::ListValue& args,
                                    std::string* out_frame) override;
  virtual Status DispatchMouseEvents(
      const std::list<MouseEvent>& events, const std::string& frame) override;
  virtual Status DispatchTouchEvent(const TouchEvent& event) override;
  virtual Status DispatchTouchEvents(
      const std::list<TouchEvent>& events) override;
  virtual Status DispatchKeyEvents(const std::list<KeyEvent>& events) override;
  virtual Status GetCookies(scoped_ptr<base::ListValue>* cookies) override;
  virtual Status DeleteCookie(const std::string& name,
                              const std::string& url) override;
  virtual Status WaitForPendingNavigations(const std::string& frame_id,
                                           const base::TimeDelta& timeout,
                                           bool stop_load_on_timeout) override;
  virtual Status IsPendingNavigation(
      const std::string& frame_id, bool* is_pending) override;
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  virtual Status OverrideGeolocation(const Geoposition& geoposition) override;
  virtual Status CaptureScreenshot(std::string* screenshot) override;
  virtual Status SetFileInputFiles(
      const std::string& frame,
      const base::DictionaryValue& element,
      const std::vector<base::FilePath>& files) override;
  virtual Status TakeHeapSnapshot(scoped_ptr<base::Value>* snapshot) override;
  virtual Status StartProfile() override;
  virtual Status EndProfile(scoped_ptr<base::Value>* profile_data) override;

 private:
  std::string id_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STUB_WEB_VIEW_H_
