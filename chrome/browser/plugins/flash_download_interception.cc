// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_features.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;

namespace {

const char kFlashDownloadURL[] = "get.adobe.com/flashplayer";

bool ShouldInterceptNavigation(
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/626728): Implement permission prompt logic.
  return true;
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle>
FlashDownloadInterception::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kPreferHtmlOverPlugins))
    return nullptr;

  if (!handle->HasUserGesture())
    return nullptr;

  if (!base::StartsWith(handle->GetURL().GetContent(), kFlashDownloadURL,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return nullptr;
  }

  return base::MakeUnique<navigation_interception::InterceptNavigationThrottle>(
      handle, base::Bind(&ShouldInterceptNavigation), true);
}
