// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_MAC_H_
#define CHROME_APP_CHROME_MAIN_MAC_H_
#pragma once

class FilePath;

// Checks if the UserDataDir policy has been set and returns its value in the
// |user_data_dir| parameter. If no policy is set the parameter is not changed.
void CheckUserDataDirPolicy(FilePath* user_data_dir);

// Sets the app bundle (base::mac::MainAppBundle()) to the framework's bundle,
// and sets the base bundle ID (base::mac::BaseBundleID()) to the proper value
// based on the running application. The base bundle ID is the outer browser
// application's bundle ID even when running in a non-browser (helper)
// process.
void SetUpBundleOverrides();

// Creates a bootstrap subset port as a subset of the current bootstrap port,
// tying its lifetime to the current task port, and switches the task's
// bootstrap port to the new bootstrap subset port. Any subsequent bootstrap
// servers created in the task via bootstrap_create_server and directed at the
// bootstrap port will be created in the bootstrap subset port, meaning that
// they will only be visible via bootstrap_look_up to this task and its
// children, and that these mappings will be destroyed along with the subset
// port as soon as this process exits.
//
// This scheme prevents bootstrap server mappings from leaking beyond this
// process' lifetime. It also prevents any mappings from being visible by any
// process other than this one and its children, but currently, nothing
// requires this behavior. If anything ever does, this function could save the
// original bootstrap port and make it available to things that need to call
// bootstrap_create_server and create mappings with the original bootstrap
// port.
//
// This needs to be called before anything calls bootstrap_create_server.
// Currently, the only things that create bootstrap server mappings are
// Breakpad and rohitfork.  To look for other users, search for
// bootstrap_create_server and -[NSMachBootstrapServer registerPort:name:].
void SwitchToMachBootstrapSubsetPort();

#endif  // CHROME_APP_CHROME_MAIN_MAC_H_
