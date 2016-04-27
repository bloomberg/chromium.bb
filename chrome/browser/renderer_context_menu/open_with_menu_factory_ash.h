// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_ASH_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_ASH_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/link_handler_model.h"

std::pair<std::unordered_map<int, ash::LinkHandlerInfo>, int>
BuildHandlersMapForTesting(const std::vector<ash::LinkHandlerInfo>& handlers);

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_ASH_H_
