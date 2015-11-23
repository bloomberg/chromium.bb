// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_CONSTANTS_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_CONSTANTS_H_

namespace ios {

// This string is a flag for net::SSLInfo signaling that the error is not a
// typical certificate error, but rather is a spoofing attempt.
// It can be used to customize the interstitial error page.
extern const char kSpoofingAttemptFlag[];

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_CONSTANTS_H_
