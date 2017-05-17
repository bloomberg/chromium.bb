// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/navigation_params.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

struct CSPViolationParams;

// A CSPContext represents the system on which the Content-Security-Policy are
// enforced. One must define via its virtual methods how to report violations
// and what is the set of scheme that bypass the CSP. Its main implementation
// is in content/browser/frame_host/render_frame_host_impl.h
class CONTENT_EXPORT CSPContext {
 public:
  CSPContext();
  virtual ~CSPContext();

  // Check if an |url| is allowed by the set of Content-Security-Policy. It will
  // report any violation by:
  // * displaying a console message.
  // * triggering the "SecurityPolicyViolation" javascript event.
  // * sending a JSON report to any uri defined with the "report-uri" directive.
  // Returns true when the request can proceed, false otherwise.
  bool IsAllowedByCsp(CSPDirective::Name directive_name,
                      const GURL& url,
                      bool is_redirect,
                      const SourceLocation& source_location);

  void SetSelf(const url::Origin origin);
  bool AllowSelf(const GURL& url);
  bool ProtocolIsSelf(const GURL& url);
  const std::string& GetSelfScheme();

  virtual void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params);

  bool SelfSchemeShouldBypassCsp();

  void ResetContentSecurityPolicies() { policies_.clear(); }
  void AddContentSecurityPolicy(const ContentSecurityPolicy& policy) {
    policies_.push_back(policy);
  }

  virtual bool SchemeShouldBypassCSP(const base::StringPiece& scheme);

  // For security reasons, some urls must not be disclosed cross-origin in
  // violation reports. This includes the blocked url and the url of the
  // initiator of the navigation. This information is potentially transmitted
  // between different renderer processes.
  // TODO(arthursonzogni): Stop hiding sensitive parts of URLs in console error
  // messages as soon as there is a way to send them to the devtools process
  // without the round trip in the renderer process.
  // See https://crbug.com/721329
  virtual void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      CSPDirective::Name directive,
      GURL* blocked_url,
      SourceLocation* source_location) const;

 private:
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
                     bool after_redirect,
                     const SourceLocation& source_location);
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

  // The source code location that triggered the blocked navigation.
  SourceLocation source_location;
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
