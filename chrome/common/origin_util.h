// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ORIGIN_UTIL_H_
#define CHROME_COMMON_ORIGIN_UTIL_H_

class GURL;

// Returns true if the origin is trustworthy: that is, if its contents can be
// said to have been transferred to the browser in a way that a network attacker
// cannot tamper with or observe.
//
// See https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
bool IsOriginSecure(const GURL& url);

#endif  // CHROME_COMMON_ORIGIN_UTIL_H_
