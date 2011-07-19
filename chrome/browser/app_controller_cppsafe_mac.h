// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_CPPSAFE_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_CPPSAFE_MAC_H_
#pragma once

// We declare this in a separate file that is safe for including in C++ code.

namespace app_controller_mac {

// True if we are currently handling an IDC_NEW_{TAB,WINDOW} command. Used in
// SessionService::Observe() to get around windows/linux and mac having
// different models of application lifetime.
bool IsOpeningNewWindow();

}  // namespace app_controller_mac

#endif  // CHROME_BROWSER_APP_CONTROLLER_CPPSAFE_MAC_H_
