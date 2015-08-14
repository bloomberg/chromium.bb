// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CONNECTION_SECURITY_H_
#define CHROME_BROWSER_SSL_CONNECTION_SECURITY_H_

#include "base/macros.h"
#include "content/public/common/security_style.h"
#include "net/cert/cert_status_flags.h"

namespace content {
class WebContents;
}  // namespace content

// This namespace contains functions responsible for computing the
// connection security status of a page.
namespace connection_security {

// These security styles describe the treatment given to pages that
// display and run mixed content. They are used to coordinate the
// treatment of mixed content with other security UI elements.
const content::SecurityStyle kDisplayedInsecureContentStyle =
    content::SECURITY_STYLE_UNAUTHENTICATED;
const content::SecurityStyle kRanInsecureContentStyle =
    content::SECURITY_STYLE_AUTHENTICATION_BROKEN;

// TODO(wtc): unify this enum with SecurityStyle.  We
// don't need two sets of security UI levels.  SECURITY_STYLE_AUTHENTICATED
// needs to be refined into three levels: warning, standard, and EV.
// See crbug.com/425728
//
// If you reorder, add, or delete values from this enum, you must also
// update the UI icons in ToolbarModelImpl::GetIconForSecurityLevel.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ssl
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ConnectionSecurityLevel
enum SecurityLevel {
  // HTTP/no URL
  NONE,

  // HTTPS with valid EV cert
  EV_SECURE,

  // HTTPS (non-EV)
  SECURE,

  // HTTPS, but unable to check certificate revocation status or with insecure
  // content on the page
  SECURITY_WARNING,

  // HTTPS, but the certificate verification chain is anchored on a
  // certificate that was installed by the system administrator
  SECURITY_POLICY_WARNING,

  // Attempted HTTPS and failed, page not authenticated
  SECURITY_ERROR,
};

// Describes how the SHA1 deprecation policy applies to an HTTPS
// connection.
enum SHA1DeprecationStatus {
  // No SHA1 deprecation policy applies.
  NO_DEPRECATED_SHA1,
  // The connection used a certificate with a SHA1 signature in the
  // chain, and policy says that the connection should be treated as
  // broken HTTPS.
  DEPRECATED_SHA1_BROKEN,
  // The connection used a certificate with a SHA1 signature in the
  // chain, and policy says that the connection should be treated with a
  // warning.
  DEPRECATED_SHA1_WARNING,
};

// Describes the type of mixed content (if any) that a site
// displayed/ran.
enum MixedContentStatus {
  NO_MIXED_CONTENT,
  // The site displayed nonsecure resources (passive mixed content).
  DISPLAYED_MIXED_CONTENT,
  // The site ran nonsecure resources (active mixed content).
  RAN_MIXED_CONTENT,
  // The site both ran and displayed nonsecure resources.
  RAN_AND_DISPLAYED_MIXED_CONTENT,
};

// Contains information about a page's security status, including a
// SecurityStyle and the information that was used to decide which
// SecurityStyle to assign.
struct SecurityInfo {
  content::SecurityStyle security_style;
  SHA1DeprecationStatus sha1_deprecation_status;
  MixedContentStatus mixed_content_status;
  net::CertStatus cert_status;
  bool scheme_is_cryptographic;
};

// Returns a security level describing the overall security state of
// the given |WebContents|.
SecurityLevel GetSecurityLevelForWebContents(
    const content::WebContents* web_contents);

// Populates |security_info| with information describing the given
// |web_contents|, including a content::SecurityStyle value and security
// properties that caused that value to be chosen.
//
// Note: This is a lossy operation. Not all of the policies
// that can be expressed by a SecurityLevel (a //chrome concept) can
// be expressed by a content::SecurityStyle.
// In general, code in //chrome should prefer to use
// GetSecurityLevelForWebContents() to determine security policy, and
// only use this function when policy needs to be supplied back to
// layers in //content.
void GetSecurityInfoForWebContents(const content::WebContents* web_contents,
                                   SecurityInfo* security_info);

}  // namespace connection_security

#endif  // CHROME_BROWSER_SSL_CONNECTION_SECURITY_H_
