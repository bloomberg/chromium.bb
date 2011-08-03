// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_CONFIGURATOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_CONFIGURATOR_H_
#pragma once

#include "chrome/browser/component_updater/component_updater_service.h"

ComponentUpdateService::Configurator* MakeChromeComponentUpdaterConfigurator(
    const CommandLine* cmdline, net::URLRequestContextGetter* context_getter);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_CONFIGURATOR_H_

