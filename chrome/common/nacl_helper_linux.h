// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NACL_HELPER_LINUX_H_
#define CHROME_COMMON_NACL_HELPER_LINUX_H_

// A mini-zygote specifically for Native Client. This file defines
// constants used to implement communication between the nacl_helper
// process and the Chrome zygote.

// Used by Helper to tell Zygote it has started successfully.
#define kNaClHelperStartupAck "NACLHELPER_OK"
// Used by Zygote to ask Helper to fork a new NaCl loader.
#define kNaClForkRequest "NACLFORK"

// The next set of constants define global Linux file descriptors.
// For communications between NaCl loader and browser.
// See also content/common/zygote_main_linux.cc and
// http://code.google.com/p/chromium/wiki/LinuxZygote

// For communications between NaCl loader and zygote.
#define kNaClZygoteDescriptor 3
// For communications between the NaCl loader process and
// the SUID sandbox.
#define kNaClSandboxDescriptor 5
// NOTE: kNaClSandboxDescriptor must match
// content/browser/zygote_main_linux.cc
// kMagicSandboxIPCDescriptor.

// A fork request from the Zygote to the helper includes an array
// of three file descriptors. These constants are used as indicies
// into the array.
// Used to pass in the descriptor for talking to the Browser
#define kNaClBrowserFDIndex 0
// The next two are used in the protocol for discovering the
// child processes real PID from within the SUID sandbox. See
// http://code.google.com/p/chromium/wiki/LinuxZygote
#define kNaClDummyFDIndex 1
#define kNaClParentFDIndex 2

#endif  // CHROME_COMMON_NACL_HELPER_LINUX_H_
