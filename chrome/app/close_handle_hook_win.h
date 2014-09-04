// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CLOSE_HANDLE_HOOK_WIN_H_
#define CHROME_APP_CLOSE_HANDLE_HOOK_WIN_H_

// Installs the hooks required to debug use of improper handles.
void InstallCloseHandleHooks();

// Removes the hooks installed by InstallCloseHandleHooks().
void RemoveCloseHandleHooks();

#endif  // CHROME_APP_CLOSE_HANDLE_HOOK_WIN_H_
