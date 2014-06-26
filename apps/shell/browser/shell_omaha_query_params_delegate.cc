// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_omaha_query_params_delegate.h"

namespace extensions {

ShellOmahaQueryParamsDelegate::ShellOmahaQueryParamsDelegate() {
}

ShellOmahaQueryParamsDelegate::~ShellOmahaQueryParamsDelegate() {
}

std::string ShellOmahaQueryParamsDelegate::GetExtraParams() {
  // This version number is high enough to be supported by Omaha
  // (below 31 is unsupported), but it's fake enough to be obviously
  // not a Chrome release.
  return "&prodversion=38.1234.5678.9";
}

}  // namespace extensions
