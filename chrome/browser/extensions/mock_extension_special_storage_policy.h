// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#pragma once

#include <set>
#include <string>

#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "googleurl/src/gurl.h"

// This class is the same as MockSpecialStoragePolicy (in
// webkit/quota/mock_special_storage_policy.h), but it inherits
// ExtensionSpecialStoragePolicy instead of quota::SpecialStoragePolicy.
class MockExtensionSpecialStoragePolicy : public ExtensionSpecialStoragePolicy {
 public:
  MockExtensionSpecialStoragePolicy();
  virtual ~MockExtensionSpecialStoragePolicy();

  virtual bool IsStorageProtected(const GURL& origin) OVERRIDE;
  virtual bool IsStorageUnlimited(const GURL& origin) OVERRIDE;
  virtual bool IsStorageSessionOnly(const GURL& origin) OVERRIDE;
  virtual bool IsFileHandler(const std::string& extension_id) OVERRIDE;
  virtual bool HasSessionOnlyOrigins() OVERRIDE;

  void AddProtected(const GURL& origin) {
    protected_.insert(origin);
  }

  void AddUnlimited(const GURL& origin) {
    unlimited_.insert(origin);
  }

  void AddSessionOnly(const GURL& origin) {
    session_only_.insert(origin);
  }

  void AddFileHandler(const std::string& id) {
    file_handlers_.insert(id);
  }

  void Reset() {
    protected_.clear();
    unlimited_.clear();
    session_only_.clear();
    file_handlers_.clear();
  }

 private:
  std::set<GURL> protected_;
  std::set<GURL> unlimited_;
  std::set<GURL> session_only_;
  std::set<std::string> file_handlers_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSpecialStoragePolicy);
};

#endif  // CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
