// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_
#define IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ios/chrome/browser/application_context.h"

class ApplicationContextImpl : public ApplicationContext {
 public:
  ApplicationContextImpl();
  ~ApplicationContextImpl() override;

  // Sets the locale used by the application.
  void SetApplicationLocale(const std::string& locale);

 private:
  // ApplicationContext implementation.
  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  const std::string& GetApplicationLocale() override;

  base::ThreadChecker thread_checker_;
  std::string application_locale_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationContextImpl);
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_
