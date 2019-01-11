// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_RENDERER_MANIFEST_H_
#define CHROME_APP_CHROME_RENDERER_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

// Returns the Manifest for the chrome_renderer service. Chrome registers an
// instance of this service for each renderer process started by Content, and
// that instance lives in the corresponding renderer process alongside the
// content_renderer instance.
const service_manager::Manifest& GetChromeRendererManifest();

#endif  // CHROME_APP_CHROME_RENDERER_MANIFEST_H_
