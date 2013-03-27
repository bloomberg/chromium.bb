// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_DATA_DIR_EXTRACTOR_H_
#define CHROME_BROWSER_USER_DATA_DIR_EXTRACTOR_H_

namespace base {
class FilePath;
}

namespace content{
struct MainFunctionParams;
}

namespace chrome {

// Returns the user data dir. Must be called prior to InitializeLocalState().
base::FilePath GetUserDataDir(const content::MainFunctionParams& parameters);

}  // namespace chrome

#endif  // CHROME_BROWSER_USER_DATA_DIR_EXTRACTOR_H_
