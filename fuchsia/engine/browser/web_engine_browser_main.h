// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_H_

#include <memory>

namespace content {
struct MainFunctionParams;
}  // namespace content

int WebEngineBrowserMain(const content::MainFunctionParams& parameters);

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_H_
