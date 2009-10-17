// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Define inet_aton alone so it's easier to include.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_INET_ATON_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_INET_ATON_H_

#if defined(OS_WIN)
int inet_aton(const char* cp, struct in_addr* inp);
#endif

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_INET_ATON_H_
