// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by the encryptor component.

#ifndef COMPONENTS_ENCRYPTOR__ENCRYPTOR_SWITCHES_H_
#define COMPONENTS_ENCRYPTOR__ENCRYPTOR_SWITCHES_H_

#include "build/build_config.h"

namespace encryptor {
namespace switches {

#if defined(OS_MACOSX)

// Uses mock keychain for testing purposes, which prevents blocking dialogs
// from causing timeouts.
extern const char kUseMockKeychain[];

#endif  // OS_MACOSX

}  // namespace switches
}  // namespace encryptor

#endif  // COMPONENTS_ENCRYPTOR__ENCRYPTOR_SWITCHES_H_
