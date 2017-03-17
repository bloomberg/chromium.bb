// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/content_security_policy_header.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// A CSPContext represents the system on which the Content-Security-Policy are
// enforced. One must define via its virtual methods how to report violations,
// how to log messages on the console and what is the set of scheme that bypass
// the CSP.
// Its main implementation is in content/browser/frame_host/csp_context_impl.h
class CONTENT_EXPORT CSPContext {
 public:
  CSPContext();
  virtual ~CSPContext();

  bool Allow(const std::vector<ContentSecurityPolicy>& policies,
             CSPDirective::Name directive_name,
             const GURL& url,
             bool is_redirect = false);

  void SetSelf(const url::Origin origin);
  bool AllowSelf(const GURL& url);
  bool ProtocolMatchesSelf(const GURL& url);

  virtual void LogToConsole(const std::string& message);
  virtual void ReportViolation(
      const std::string& directive_text,
      const std::string& effective_directive,
      const std::string& message,
      const GURL& blocked_url,
      const std::vector<std::string>& report_end_points,
      const std::string& header,
      blink::WebContentSecurityPolicyType disposition);

  bool SelfSchemeShouldBypassCSP();

 private:
  virtual bool SchemeShouldBypassCSP(const base::StringPiece& scheme);

  bool has_self_ = false;
  std::string self_scheme_;
  CSPSource self_source_;

  DISALLOW_COPY_AND_ASSIGN(CSPContext);
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
