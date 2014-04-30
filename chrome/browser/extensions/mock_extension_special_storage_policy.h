// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_

#include <set>
#include <string>

#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "url/gurl.h"

// This class is the same as MockSpecialStoragePolicy (in
// content/public/test/mock_special_storage_policy.h), but it inherits
// ExtensionSpecialStoragePolicy instead of quota::SpecialStoragePolicy.
class MockExtensionSpecialStoragePolicy : public ExtensionSpecialStoragePolicy {
 public:
  MockExtensionSpecialStoragePolicy();

  // quota::SpecialStoragePolicy:
  virtual bool IsStorageProtected(const GURL& origin) OVERRIDE;
  virtual bool IsStorageUnlimited(const GURL& origin) OVERRIDE;
  virtual bool IsStorageSessionOnly(const GURL& origin) OVERRIDE;
  virtual bool CanQueryDiskSize(const GURL& origin) OVERRIDE;
  virtual bool IsFileHandler(const std::string& extension_id) OVERRIDE;
  virtual bool HasSessionOnlyOrigins() OVERRIDE;

  void AddProtected(const GURL& origin) {
    protected_.insert(origin);
  }

 private:
  virtual ~MockExtensionSpecialStoragePolicy();

  std::set<GURL> protected_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSpecialStoragePolicy);
};

#endif  // CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
