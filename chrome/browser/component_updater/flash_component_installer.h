// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_FLASH_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_FLASH_COMPONENT_INSTALLER_H_
#pragma once

class ComponentUpdateService;
class FilePath;

namespace base {
class DictionaryValue;
}

// Our job is to 1) find what pepper flash is installed (if any) and 2) register
// with the component updater to download the latest version when available.
// The first part is IO intensive so we do it asynchronously in the file thread.
void RegisterPepperFlashComponent(ComponentUpdateService* cus);

// Returns true if the this browser implements all the interfaces that flash
// specifies in its component installer manifest.
bool VetoPepperFlashIntefaces(base::DictionaryValue* manifest);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_FLASH_COMPONENT_INSTALLER_H_
