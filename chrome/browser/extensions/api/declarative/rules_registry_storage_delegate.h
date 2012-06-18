// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_STORAGE_DELEGATE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_STORAGE_DELEGATE_H__
#pragma once

#include <string>

#include "base/memory/ref_counted.h"

#include "chrome/browser/extensions/api/declarative/rules_registry_with_cache.h"

class Profile;

namespace extensions {

// A Delegate to the RulesRegistryWithCache which handles reading/writing rules
// to the extension state store. This class should be initialized on the UI
// thread, but used on the RulesRegistry thread.
class RulesRegistryStorageDelegate : public RulesRegistryWithCache::Delegate {
 public:
  RulesRegistryStorageDelegate();
  virtual ~RulesRegistryStorageDelegate();

  // Called on the UI thread to initialize the delegate.
  void Init(Profile* profile,
            RulesRegistryWithCache* rules_registry,
            const std::string& storage_key);

  // RulesRegistryWithCache::Delegate
  virtual bool IsReady() OVERRIDE;
  virtual void OnRulesChanged(RulesRegistryWithCache* rules_registry,
                              const std::string& extension_id) OVERRIDE;

 private:
  class Inner;

  scoped_refptr<Inner> inner_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_STORAGE_DELEGATE_H__
