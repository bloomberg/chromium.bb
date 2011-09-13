// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_MAIN_H_
#define CONTENT_RENDERER_RENDERER_MAIN_H_
#pragma once

#include "content/common/content_export.h"

struct MainFunctionParams;

// This is the mainline routine for running as the Renderer process.
CONTENT_EXPORT int RendererMain(const MainFunctionParams& parameters);

#endif  // CONTENT_RENDERER_RENDERER_MAIN_H_
