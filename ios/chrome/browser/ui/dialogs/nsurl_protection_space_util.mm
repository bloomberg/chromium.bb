// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace nsurlprotectionspace_util {

NSString* MessageForHTTPAuth(NSURLProtectionSpace* protectionSpace) {
  DCHECK(CanShow(protectionSpace));

  if (protectionSpace.receivesCredentialSecurely)
    return RequesterIdentity(protectionSpace);

  NSString* securityWarning =
      l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_SUMMARY);
  return
      [NSString stringWithFormat:@"%@ %@", RequesterIdentity(protectionSpace),
                                 securityWarning];
}

BOOL CanShow(NSURLProtectionSpace* protectionSpace) {
  if (protectionSpace.host.length == 0)
    return NO;

  if (!base::IsValueInRangeForNumericType<uint16_t>(protectionSpace.port))
    return NO;  // Port is invalid.

  if (!protectionSpace.isProxy && !RequesterOrigin(protectionSpace).is_valid())
    return NO;  // Can't construct origin for non-proxy requester.

  return YES;
}

NSString* RequesterIdentity(NSURLProtectionSpace* protectionSpace) {
  GURL requesterOrigin = RequesterOrigin(protectionSpace);
  int formatID = protectionSpace.isProxy ? IDS_LOGIN_DIALOG_PROXY_AUTHORITY
                                         : IDS_LOGIN_DIALOG_AUTHORITY;
  if (!requesterOrigin.is_valid()) {
    // May be invalid for SOCKS proxy type.
    return l10n_util::GetNSStringF(
        formatID, base::SysNSStringToUTF16(protectionSpace.host));
  }
  base::string16 authority =
      url_formatter::FormatUrlForSecurityDisplay(requesterOrigin);

  return l10n_util::GetNSStringF(formatID, authority);
}

GURL RequesterOrigin(NSURLProtectionSpace* protectionSpace) {
  std::string scheme = base::SysNSStringToUTF8(protectionSpace.protocol);
  std::string host = base::SysNSStringToUTF8(protectionSpace.host);
  uint16_t port = base::checked_cast<uint16_t>(protectionSpace.port);

  return GURL(url::SchemeHostPort(scheme, host, port).Serialize());
}

}  // namespace nsurlprotectionspace_util
