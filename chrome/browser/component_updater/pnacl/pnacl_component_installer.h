// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_COMPONENT_INSTALLER_H_

class CommandLine;
class ComponentUpdateService;
class Version;

namespace base {
class DictionaryValue;
}

// Component update registration for Portable Native Client.
void RegisterPnaclComponent(ComponentUpdateService* cus,
                            const CommandLine& command_line);

// Returns true if this browser is compatible with the given Pnacl component
// manifest, with the version specified in the manifest in |version_out|.
bool CheckPnaclComponentManifest(base::DictionaryValue* manifest,
                                 Version* version_out);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_COMPONENT_INSTALLER_H_
