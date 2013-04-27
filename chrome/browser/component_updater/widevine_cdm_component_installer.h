// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_WIDEVINE_CDM_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_WIDEVINE_CDM_COMPONENT_INSTALLER_H_

class ComponentUpdateService;

// Our job is to 1) find what Widevine CDM is installed (if any) and 2) register
// with the component updater to download the latest version when available.
// The first part is IO intensive so we do it asynchronously in the file thread.
void RegisterWidevineCdmComponent(ComponentUpdateService* cus);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_WIDEVINE_CDM_COMPONENT_INSTALLER_H_
