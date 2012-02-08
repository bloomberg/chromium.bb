// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AddRulesFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.declarative.addRules");
};

class RemoveRulesFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.declarative.removeRules");
};

class GetRulesFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.declarative.getRules");
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H__
