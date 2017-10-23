// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//  Developers:
//  - All contents of the whitelist project must be compatible
//    with chrome_elf (early loading framework) requirements.
//
//  - IMPORTANT: all code executed during Init() must be meet the restrictions
//    for DllMain on Windows.
//    https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971.aspx

#ifndef CHROME_ELF_WHITELIST_WHITELIST_H_
#define CHROME_ELF_WHITELIST_WHITELIST_H_

namespace whitelist {

// Whitelist support is enabled and initialized in this process.
bool IsWhitelistInitialized();

// Init Whitelist
// --------------
// Central initialization for all whitelist sources. Users of the whitelist
// only need to include this file, and call this one function at startup.
//
// Initializes the DLL whitelist in the current process. This should be called
// before any undesirable DLLs might be loaded.
bool Init();

}  // namespace whitelist

#endif  // CHROME_ELF_WHITELIST_WHITELIST_H_
