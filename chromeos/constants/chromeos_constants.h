// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants related to ChromeOS.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_CONSTANTS_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace chromeos {

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FilePath::CharType kDriveCacheDirname[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FilePath::CharType kNssCertDbPath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FilePath::CharType kNssDirPath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FilePath::CharType kNssKeyDbPath[];

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_CONSTANTS_H_
