// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities related to installation of the CEEE.

#ifndef CEEE_COMMON_INSTALL_UTILS_H_
#define CEEE_COMMON_INSTALL_UTILS_H_

// This is the only way we define DllRegisterServer in CEEE.
// It guarantees that we check whether registration should be
// done before actually doing it.
//
// Instructions:
//
// a) In your module's main compilation unit, define
// a function DllRegisterServerImpl with the same signature
// as DllRegisterServer.
//
// b) Below your DllRegisterServerImpl function, put this
// macro.
#define CEEE_DEFINE_DLL_REGISTER_SERVER() \
    STDAPI DllRegisterServer(void) { \
      if (!ceee_install_utils::ShouldRegisterCeee()) { \
        return S_OK; \
      } else { \
        return DllRegisterServerImpl(); \
      } \
    }

namespace ceee_install_utils {

// Returns true if the --enable-ceee flag was passed to the process
// that loaded this DLL, or if the process loading the DLL is
// regsvr32 (i.e. it's a developer that explicitly wants to register).
bool ShouldRegisterCeee();

// Returns true if the --enable-ff-ceee flag was passed to the process
// that loaded this DLL in addition to the --enable-ceee flag, or if
// the process loading the DLL is regsvr32 (i.e. it's a developer that
// explicitly wants to register).
bool ShouldRegisterFfCeee();

}  // namespace ceee_install_utils

#endif  // CEEE_COMMON_INSTALL_UTILS_H_
