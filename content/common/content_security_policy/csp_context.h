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

struct CSPViolationParams;

// A CSPContext represents the system on which the Content-Security-Policy are
// enforced. One must define via its virtual methods how to report violations,
// how to log messages on the console and what is the set of scheme that bypass
// the CSP. Its main implementation is in
// content/browser/frame_host/render_frame_host_impl.h
class CONTENT_EXPORT CSPContext {
 public:
  CSPContext();
  virtual ~CSPContext();

  bool IsAllowedByCsp(CSPDirective::Name directive_name,
                      const GURL& url,
                      bool is_redirect = false);

  void SetSelf(const url::Origin origin);
  bool AllowSelf(const GURL& url);
  bool ProtocolMatchesSelf(const GURL& url);

  virtual void LogToConsole(const std::string& message);
  virtual void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params);

  bool SelfSchemeShouldBypassCsp();

  void ResetContentSecurityPolicies() { policies_.clear(); }
  void AddContentSecurityPolicy(const ContentSecurityPolicy& policy) {
    policies_.push_back(policy);
  }

 private:
  virtual bool SchemeShouldBypassCSP(const base::StringPiece& scheme);

  bool has_self_ = false;
  std::string self_scheme_;
  CSPSource self_source_;

  std::vector<ContentSecurityPolicy> policies_;

  DISALLOW_COPY_AND_ASSIGN(CSPContext);
};

// Used in CSPContext::ReportViolation()
struct CONTENT_EXPORT CSPViolationParams {
  CSPViolationParams();
  CSPViolationParams(const std::string& directive,
                     const std::string& effective_directive,
                     const std::string& console_message,
                     const GURL& blocked_url,
                     const std::vector<std::string>& report_endpoints,
                     const std::string& header,
                     const blink::WebContentSecurityPolicyType& disposition,
                     bool after_redirect);
  CSPViolationParams(const CSPViolationParams& other);
  ~CSPViolationParams();

  // The name of the directive that violates the policy. |directive| might be a
  // directive that serves as a fallback to the |effective_directive|.
  std::string directive;

  // The name the effective directive that was checked against.
  std::string effective_directive;

  // The console message to be displayed to the user.
  std::string console_message;

  // The URL that was blocked by the policy.
  GURL blocked_url;

  // The set of URI where a JSON-formatted report of the violation should be
  // sent.
  std::vector<std::string> report_endpoints;

  // The raw content security policy header that was violated.
  std::string header;

  // Each policy has an associated disposition, which is either "enforce" or
  // "report".
  blink::WebContentSecurityPolicyType disposition;

  // Whether or not the violation happens after a redirect.
  bool after_redirect;
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
