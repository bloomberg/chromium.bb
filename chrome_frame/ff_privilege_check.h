// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_FF_PRIVILEGE_CHECK_H_
#define CHROME_FRAME_FF_PRIVILEGE_CHECK_H_

// Returns true iff we're being invoked in a privileged document
// in FireFox 3.x.
// An example privileged document is when the plugin is instantiated
// in browser chrome by XUL.
bool IsFireFoxPrivilegedInvocation(NPP instance);

#endif  // CHROME_FRAME_FF_PRIVILEGE_CHECK_H_
