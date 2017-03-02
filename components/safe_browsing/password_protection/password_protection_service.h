// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

class GURL;

namespace safe_browsing {

class SafeBrowsingDatabaseManager;

class PasswordProtectionService {
 public:
  explicit PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager);

  virtual ~PasswordProtectionService();

  // Check if |url| matches CSD whitelist and record UMA metric accordingly.
  // Currently called by PasswordReuseDetectionManager on UI thread.
  void RecordPasswordReuse(const GURL& url);

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // Called on UI thread.
  // Increases "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist" UMA
  // metric based on input.
  void OnMatchCsdWhiteListResult(bool match_whitelist);

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtrFactory<PasswordProtectionService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
