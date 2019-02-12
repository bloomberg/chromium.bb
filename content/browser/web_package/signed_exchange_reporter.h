// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REPORTER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REPORTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace network {
struct ResourceResponseHead;
}  // namespace network

namespace content {

class CONTENT_EXPORT SignedExchangeReporter {
 public:
  static std::unique_ptr<SignedExchangeReporter> MaybeCreate(
      const GURL& outer_url,
      const std::string& referrer,
      const network::ResourceResponseHead& response,
      base::OnceCallback<int(void)> frame_tree_node_id_getter);

  ~SignedExchangeReporter();

  void set_cert_server_ip(const std::string& cert_server_ip);
  void set_inner_url(const GURL& inner_url);
  void set_cert_url(const GURL& cert_url);

  void ReportResult(SignedExchangeLoadResult result);

 private:
  SignedExchangeReporter(
      const GURL& outer_url,
      const std::string& referrer,
      const network::ResourceResponseHead& response,
      base::OnceCallback<int(void)> frame_tree_node_id_getter);

  const GURL outer_url_;
  const std::string referrer_;
  const std::string server_ip_;
  const int status_code_;
  base::OnceCallback<int(void)> frame_tree_node_id_getter_;
  std::string cert_server_ip_;
  GURL inner_url_;
  GURL cert_url_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeReporter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REPORTER_H_
