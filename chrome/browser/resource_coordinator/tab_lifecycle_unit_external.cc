// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

namespace resource_coordinator {

// static
TabLifecycleUnitExternal* TabLifecycleUnitExternal::FromWebContents(
    content::WebContents* web_contents) {
  TabLifecycleUnitSource* source = TabLifecycleUnitSource::GetInstance();
  if (!source)
    return nullptr;
  return source->GetTabLifecycleUnitExternal(web_contents);
}

}  // namespace resource_coordinator
