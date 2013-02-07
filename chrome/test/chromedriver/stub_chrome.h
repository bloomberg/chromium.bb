// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_STUB_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_STUB_CHROME_H_

#include <list>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome.h"

class Status;

class StubChrome : public Chrome {
 public:
  StubChrome();
  virtual ~StubChrome();

  // Overridden from Chrome:
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
  virtual Status Quit() OVERRIDE;
  virtual Status WaitForPendingNavigations(
      const std::string& frame_id) OVERRIDE;
  virtual Status GetMainFrame(std::string* frame_id) OVERRIDE;
};

#endif  // CHROME_TEST_CHROMEDRIVER_STUB_CHROME_H_
