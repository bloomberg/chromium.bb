// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_STARTUP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_STARTUP_H_
#pragma once

class CommandLine;
class Profile;

// Initialization helpers for various Extension startup actions.
namespace extensions_startup {
// Handle --pack-extension flag from the |cmd_line| by packing the specified
// extension. Returns false if the pack job could not be started.
bool HandlePackExtension(const CommandLine& cmd_line);

// Handle --uninstall-extension flag from the |cmd_line| by uninstalling the
// specified extension from |profile|. Returns false if the uninstall job
// could not be started.
bool HandleUninstallExtension(const CommandLine& cmd_line, Profile* profile);
}  // namespace extensions_startup

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_STARTUP_H_
