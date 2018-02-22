// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_UTIL_H_

// Returns true if crostini is allowed to run.
// Otherwise, returns false, e.g. if crostini is not available on the device,
// or it is in the flow to set up managed account creation.
bool IsCrostiniAllowed();

// Returns true if crostini UI can be shown. Implies crostini is allowed to run.
bool IsExperimentalCrostiniUIAvailable();

#endif  // CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_UTIL_H_
