// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_CHROME_WIN_H_
#define CHROME_BROWSER_UI_METRO_CHROME_WIN_H_

namespace chrome {

// Using IApplicationActivationManager::ActivateApplication, activate the
// Chrome window running in Metro mode. Returns true if the activation was
// successful. Note that this can not be called nested in another COM
// SendMessage (results in error RPC_E_CANTCALLOUT_ININPUTSYNCCALL), so use
// PostTask to handle that case.
bool ActivateMetroChrome();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_METRO_CHROME_WIN_H_
