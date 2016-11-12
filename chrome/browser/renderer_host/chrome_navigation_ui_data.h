// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/navigation_ui_data.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/features/features.h"

namespace content {
class NavigationHandle;
}

// PlzNavigate
// Contains data that is passed from the UI thread to the IO thread at the
// beginning of each navigation. The class is instantiated on the UI thread,
// then a copy created using Clone is passed to the content::ResourceRequestInfo
// on the IO thread.
class ChromeNavigationUIData : public content::NavigationUIData {
 public:
  ChromeNavigationUIData();
  explicit ChromeNavigationUIData(content::NavigationHandle* navigation_handle);
  ~ChromeNavigationUIData() override;

  // Creates a new ChromeNavigationUIData that is a deep copy of the original.
  // Any changes to the original after the clone is created will not be
  // reflected in the clone.  |extension_data_| is deep copied.
  std::unique_ptr<content::NavigationUIData> Clone() const override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetExtensionNavigationUIData(
      std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data);

  extensions::ExtensionNavigationUIData* GetExtensionNavigationUIData() const {
    return extension_data_.get();
  }
#endif

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Manages the lifetime of optional ExtensionNavigationUIData information.
  std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeNavigationUIData);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_
