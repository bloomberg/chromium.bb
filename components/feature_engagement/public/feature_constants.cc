/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_constants.h"

#include "components/feature_engagement/features.h"

namespace feature_engagement {

const base::Feature kIPHDemoMode{"IPH_DemoMode",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIPHDummyFeature{"IPH_Dummy",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kIPHDataSaverDetailFeature{
    "IPH_DataSaverDetail", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDataSaverPreviewFeature{
    "IPH_DataSaverPreview", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadHomeFeature{"IPH_DownloadHome",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageFeature{"IPH_DownloadPage",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageScreenshotFeature{
    "IPH_DownloadPageScreenshot", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeHomeExpandFeature{
    "IPH_ChromeHomeExpand", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeHomeMenuHeaderFeature{
    "IPH_ChromeHomeMenuHeader", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHMediaDownloadFeature{"IPH_MediaDownload",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchFeature{
    "IPH_ContextualSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchTapFeature{
    "IPH_ContextualSearchTap", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchPanelFeature{
    "IPH_ContextualSearchPanel", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchOptInFeature{
    "IPH_ContextualSearchOptIn", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
const base::Feature kIPHBookmarkFeature{"IPH_Bookmark",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHIncognitoWindowFeature{
    "IPH_IncognitoWindow", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHNewTabFeature{"IPH_NewTab",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_DESKTOP_IPH)

#if defined(OS_IOS)
const base::Feature kIPHNewTabTipFeature{"IPH_NewTabTip",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHNewIncognitoTabTipFeature{
    "IPH_NewIncognitoTabTip", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHBadgedReadingListFeature{
    "IPH_BadgedReadingList", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_IOS)

}  // namespace feature_engagement
