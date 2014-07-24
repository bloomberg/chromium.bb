// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_OMAHA_QUERY_PARAMS_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_OMAHA_QUERY_PARAMS_DELEGATE_H_

#include "components/omaha_query_params/omaha_query_params_delegate.h"

namespace extensions {

class ShellOmahaQueryParamsDelegate
    : public omaha_query_params::OmahaQueryParamsDelegate {
 public:
  ShellOmahaQueryParamsDelegate();
  virtual ~ShellOmahaQueryParamsDelegate();

  virtual std::string GetExtraParams() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellOmahaQueryParamsDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_OMAHA_QUERY_PARAMS_DELEGATE_H_
