// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EMBEDDED_UI_SERVICE_INFO_FACTORY_H_
#define CHROME_BROWSER_EMBEDDED_UI_SERVICE_INFO_FACTORY_H_

#include "services/service_manager/embedder/embedded_service_info.h"
#include "services/ui/common/image_cursors_set.h"

// Returns an EmbeddedServiceInfo used to create the UI service.
service_manager::EmbeddedServiceInfo CreateEmbeddedUIServiceInfo(
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr);

#endif  // CHROME_BROWSER_EMBEDDED_UI_SERVICE_INFO_FACTORY_H_
