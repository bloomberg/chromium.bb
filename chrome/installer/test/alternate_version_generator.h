// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file exposes the public interface to the mini_installer re-versioner.

#ifndef CHROME_INSTALLER_TEST_ALTERNATE_VERSION_GENERATOR_H_
#define CHROME_INSTALLER_TEST_ALTERNATE_VERSION_GENERATOR_H_
#pragma once

#include <string>

class FilePath;

namespace upgrade_test {

enum Direction {
  PREVIOUS_VERSION,
  NEXT_VERSION
};

// Generates an alternate mini_installer.exe using the one indicated by
// |original_installer_path|, giving the new one a lower or higher version than
// the original and placing it in |target_path|.  Any previous file at
// |target_path| is clobbered.  Returns true on success.  |original_version| and
// |new_version|, when non-NULL, are given the original and new version numbers
// on success.
bool GenerateAlternateVersion(const FilePath& original_installer_path,
                              const FilePath& target_path,
                              Direction direction,
                              std::wstring* original_version,
                              std::wstring* new_version);

}  // namespace upgrade_test

#endif  // CHROME_INSTALLER_TEST_ALTERNATE_VERSION_GENERATOR_H_
