// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.components.module_installer.ModuleInterface;

/**
 * An interface whose implementation is in dynamic module. The
 * implementation is loaded via reflection from base module to check
 * if dynamic module is installed.
 */
@ModuleInterface(module = "autofill_assistant",
        impl = "org.chromium.chrome.browser.autofill_assistant.DynamicModuleCheckImpl")
public interface DynamicModuleCheck {}
