// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_MAC_H_
#define CHROME_APP_CHROME_MAIN_MAC_H_

// Sets the app bundle (base::mac::FrameworkBundle()) to the framework's bundle,
// and sets the base bundle ID (base::mac::BaseBundleID()) to the proper value
// based on the running application. The base bundle ID is the outer browser
// application's bundle ID even when running in a non-browser (helper)
// process.
void SetUpBundleOverrides();

#endif  // CHROME_APP_CHROME_MAIN_MAC_H_
