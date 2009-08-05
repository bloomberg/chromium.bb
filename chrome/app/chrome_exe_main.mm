// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all invocations of Chromium, browser and renderer. On
// windows, this does nothing but load chrome.dll and invoke its entry point
// in order to make it easy to update the app from GoogleUpdate. We don't need
// that extra layer with Keystone on the Mac, though we may run into issues
// with Keychain prompts unless we sign the application. That shouldn't be
// too hard, we just need infrastructure support to do it.

extern "C" {
int ChromeMain(int argc, const char** argv);
}

int main(int argc, const char** argv) {
  return ChromeMain(argc, argv);
}
