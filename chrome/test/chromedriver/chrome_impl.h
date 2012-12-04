// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "chrome/test/chromedriver/chrome.h"

class DevToolsClient;
class Status;
class URLRequestContextGetter;

class ChromeImpl : public Chrome {
 public:
  ChromeImpl(base::ProcessHandle process,
             URLRequestContextGetter* context_getter,
             base::ScopedTempDir* user_data_dir,
             int port);
  virtual ~ChromeImpl();

  Status Init();

  // Overridden from Chrome:
  virtual Status Load(const std::string& url) OVERRIDE;
  virtual Status Quit() OVERRIDE;

 private:
  base::ProcessHandle process_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  base::ScopedTempDir user_data_dir_;
  int port_;
  scoped_ptr<DevToolsClient> client_;
};

namespace internal {

Status ParsePagesInfo(const std::string& data,
                      std::list<std::string>* debugger_urls);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_
