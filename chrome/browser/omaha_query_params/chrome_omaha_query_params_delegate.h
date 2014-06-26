// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMAHA_QUERY_PARAMS_CHROME_OMAHA_QUERY_PARAMS_DELEGATE_H_
#define CHROME_BROWSER_OMAHA_QUERY_PARAMS_CHROME_OMAHA_QUERY_PARAMS_DELEGATE_H_

#include "components/omaha_query_params/omaha_query_params_delegate.h"

class ChromeOmahaQueryParamsDelegate
    : public omaha_query_params::OmahaQueryParamsDelegate {
 public:
  ChromeOmahaQueryParamsDelegate();
  virtual ~ChromeOmahaQueryParamsDelegate();

  // Gets the LazyInstance for ChromeOmahaQueryParamsDelegate.
  static ChromeOmahaQueryParamsDelegate* GetInstance();

  // omaha_query_params::OmahaQueryParamsDelegate:
  virtual std::string GetExtraParams() OVERRIDE;

  // Returns the value we use for the "updaterchannel=" and "prodchannel="
  // parameters. Possible return values include: "canary", "dev", "beta", and
  // "stable".
  static const char* GetChannelString();

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  static const char* GetLang();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOmahaQueryParamsDelegate);
};

#endif  // CHROME_BROWSER_OMAHA_QUERY_PARAMS_CHROME_OMAHA_QUERY_PARAMS_DELEGATE_H_
