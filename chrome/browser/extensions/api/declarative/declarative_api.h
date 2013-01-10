// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H__

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class RulesFunction : public AsyncExtensionFunction {
 public:
  RulesFunction();

 protected:
  virtual ~RulesFunction();

  // ExtensionFunction:
  virtual bool HasPermission() OVERRIDE;
  virtual bool RunImpl() OVERRIDE;

  // Concrete implementation of the RulesFunction that is being called
  // on the thread on which the respective RulesRegistry lives.
  // Returns false in case of errors.
  virtual bool RunImplOnCorrectThread() = 0;

  scoped_refptr<RulesRegistry> rules_registry_;
};

class AddRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("events.addRules");

 protected:
  virtual ~AddRulesFunction() {}

  // RulesFunction:
  virtual bool RunImplOnCorrectThread() OVERRIDE;
};

class RemoveRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("events.removeRules");

 protected:
  virtual ~RemoveRulesFunction() {}

  // RulesFunction:
  virtual bool RunImplOnCorrectThread() OVERRIDE;
};

class GetRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("events.getRules");

 protected:
  virtual ~GetRulesFunction() {}

  // RulesFunction:
  virtual bool RunImplOnCorrectThread() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H__
