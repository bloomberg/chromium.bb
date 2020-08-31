// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "content/browser/devtools/protocol/browser.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

class BrowserContext;

namespace protocol {

class BrowserHandler : public DevToolsDomainHandler, public Browser::Backend {
 public:
  explicit BrowserHandler(bool allow_set_download_behavior);
  ~BrowserHandler() override;

  static Response FindBrowserContext(
      const Maybe<std::string>& browser_context_id,
      BrowserContext** browser_context);

  void Wire(UberDispatcher* dispatcher) override;

  Response Disable() override;

  // Protocol methods.
  Response GetVersion(std::string* protocol_version,
                      std::string* product,
                      std::string* revision,
                      std::string* user_agent,
                      std::string* js_version) override;

  Response GetHistograms(
      Maybe<std::string> in_query,
      Maybe<bool> in_delta,
      std::unique_ptr<Array<Browser::Histogram>>* histograms) override;

  Response GetHistogram(
      const std::string& in_name,
      Maybe<bool> in_delta,
      std::unique_ptr<Browser::Histogram>* out_histogram) override;

  Response GetBrowserCommandLine(
      std::unique_ptr<protocol::Array<std::string>>* arguments) override;

  Response SetPermission(
      std::unique_ptr<protocol::Browser::PermissionDescriptor> permission,
      const protocol::Browser::PermissionSetting& setting,
      Maybe<std::string> origin,
      Maybe<std::string> browser_context_id) override;

  Response GrantPermissions(
      std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
          permissions,
      Maybe<std::string> origin,
      Maybe<std::string> browser_context_id) override;

  Response ResetPermissions(Maybe<std::string> browser_context_id) override;

  Response SetDownloadBehavior(const std::string& behavior,
                               Maybe<std::string> browser_context_id,
                               Maybe<std::string> download_path) override;
  Response DoSetDownloadBehavior(const std::string& behavior,
                                 BrowserContext* browser_context,
                                 Maybe<std::string> download_path);

  Response Crash() override;
  Response CrashGpuProcess() override;

 private:
  base::flat_set<std::string> contexts_with_overridden_permissions_;
  base::flat_set<std::string> contexts_with_overridden_downloads_;
  const bool allow_set_download_behavior_;

  DISALLOW_COPY_AND_ASSIGN(BrowserHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
