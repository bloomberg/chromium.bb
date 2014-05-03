// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class RulesFunction : public ChromeAsyncExtensionFunction {
 public:
  RulesFunction();

 protected:
  virtual ~RulesFunction();

  // ExtensionFunction:
  virtual bool HasPermission() OVERRIDE;
  virtual bool RunAsync() OVERRIDE;

  // Concrete implementation of the RulesFunction that is being called
  // on the thread on which the respective RulesRegistry lives.
  // Returns false in case of errors.
  virtual bool RunAsyncOnCorrectThread() = 0;

  scoped_refptr<RulesRegistry> rules_registry_;
};

class EventsEventAddRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("events.addRules", EVENTS_ADDRULES)

 protected:
  virtual ~EventsEventAddRulesFunction() {}

  // RulesFunction:
  virtual bool RunAsyncOnCorrectThread() OVERRIDE;
};

class EventsEventRemoveRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("events.removeRules", EVENTS_REMOVERULES)

 protected:
  virtual ~EventsEventRemoveRulesFunction() {}

  // RulesFunction:
  virtual bool RunAsyncOnCorrectThread() OVERRIDE;
};

class EventsEventGetRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("events.getRules", EVENTS_GETRULES)

 protected:
  virtual ~EventsEventGetRulesFunction() {}

  // RulesFunction:
  virtual bool RunAsyncOnCorrectThread() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_H_
