// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_TEST_UTIL_H_

namespace base {
class FilePath;
}

namespace extensions {

extern const char kTestNativeMessagingHostName[];
extern const char kTestNativeMessagingExtensionId[];

void CreateTestNativeHostManifest(base::FilePath manifest_path);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_TEST_UTIL_H_
