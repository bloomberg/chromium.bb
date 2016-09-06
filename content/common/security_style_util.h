// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SECURITY_STYLE_UTIL_H_
#define CONTENT_COMMON_SECURITY_STYLE_UTIL_H_

#include "content/public/common/security_style.h"
#include "net/cert/cert_status_flags.h"

class GURL;

namespace content {

// Returns a security style describing an individual resource. Does
// not take into account any of the page- or host-level state such as
// mixed content or whether the host has run insecure content.
SecurityStyle GetSecurityStyleForResource(const GURL& url,
                                          bool has_certificate,
                                          net::CertStatus cert_status);

}  // namespace content

#endif  // CONTENT_COMMON_SECURITY_STYLE_UTIL_H_
