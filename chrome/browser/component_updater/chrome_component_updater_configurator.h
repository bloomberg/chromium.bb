// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CHROME_COMPONENT_UPDATER_CONFIGURATOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CHROME_COMPONENT_UPDATER_CONFIGURATOR_H_

#include "components/component_updater/component_updater_configurator.h"

namespace base {
class CommandLine;
}

namespace net {
class URLRequestContextGetter;
}

namespace component_updater {

class Configurator;

Configurator* MakeChromeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CHROME_COMPONENT_UPDATER_CONFIGURATOR_H_
