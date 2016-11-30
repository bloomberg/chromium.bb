// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_VERIFIER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_VERIFIER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "extensions/browser/content_verifier_delegate.h"

namespace content {
class BrowserContext;
}

namespace net {
class BackoffEntry;
}

namespace extensions {

class ChromeContentVerifierDelegate : public ContentVerifierDelegate {
 public:
  static Mode GetDefaultMode();

  explicit ChromeContentVerifierDelegate(content::BrowserContext* context);

  ~ChromeContentVerifierDelegate() override;

  // ContentVerifierDelegate:
  Mode ShouldBeVerified(const Extension& extension) override;
  ContentVerifierKey GetPublicKey() override;
  GURL GetSignatureFetchUrl(const std::string& extension_id,
                            const base::Version& version) override;
  std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) override;
  void VerifyFailed(const std::string& extension_id,
                    ContentVerifyJob::FailureReason reason) override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(ContentVerifierPolicyTest, Backoff);
  // For tests, overrides the default action to take to initiate policy
  // force-reinstalls.
  static void set_policy_reinstall_action_for_test(
      base::Callback<void(base::TimeDelta delay)>* action);

 private:
  content::BrowserContext* context_;
  ContentVerifierDelegate::Mode default_mode_;

  // This maps an extension id to a backoff entry for slowing down
  // redownload/reinstall of corrupt policy extensions if it keeps happening
  // in a loop (eg crbug.com/661738).
  std::map<std::string, std::unique_ptr<net::BackoffEntry>>
      policy_reinstall_backoff_;

  // For reporting metrics in BOOTSTRAP mode, when an extension would be
  // disabled if content verification was in ENFORCE mode.
  std::set<std::string> would_be_disabled_ids_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentVerifierDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_VERIFIER_DELEGATE_H_
