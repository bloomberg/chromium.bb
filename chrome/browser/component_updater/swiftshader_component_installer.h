// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SWIFTSHADER_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SWIFTSHADER_COMPONENT_INSTALLER_H_

namespace component_updater {

class ComponentUpdateService;

// Our job is to 1) find what version of SwiftShader is installed (if any)
// and 2) register with the gpu data manager and then register with the
// component updater if the current gpu is blacklisted.
void RegisterSwiftShaderComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SWIFTSHADER_COMPONENT_INSTALLER_H_
