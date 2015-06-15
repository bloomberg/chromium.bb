// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CONNECTION_SECURITY_H_
#define CHROME_BROWSER_SSL_CONNECTION_SECURITY_H_

#include "base/macros.h"
#include "content/public/common/security_style.h"

namespace content {
class WebContents;
}  // namespace content

// This namespace contains functions responsible for computing the
// connection security status of a page.
namespace connection_security {

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

// Returns a security level describing the overall security state of
// the given |WebContents|.
SecurityLevel GetSecurityLevelForWebContents(
    const content::WebContents* web_contents);

// Returns the content::SecurityStyle for the given |web_contents|.
// Note: This is a lossy operation. Not all of the policies
// that can be expressed by a SecurityLevel (a //chrome concept) can
// be expressed by a content::SecurityStyle.
// In general, code in //chrome should prefer to use
// GetSecurityLevelForWebContents() to determine security policy, and
// only use this function when policy needs to be supplied back to
// layers in //content.
content::SecurityStyle GetSecurityStyleForWebContents(
    const content::WebContents* web_contents);

}  // namespace connection_security

#endif  // CHROME_BROWSER_SSL_CONNECTION_SECURITY_H_
