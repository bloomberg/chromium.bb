// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CAPS_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CAPS_INSTALLER_WIN_H_

namespace component_updater {

class ComponentUpdateService;

// Call once during startup to register CAPS for updates.
void RegisterCAPSComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CAPS_INSTALLER_WIN_H_
