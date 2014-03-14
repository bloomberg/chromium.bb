// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_METRICS_IDS_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_METRICS_IDS_H_

namespace banners {

#define DEFINE_APP_BANNER_METRICS_ID_ENUM_START(identifier) enum identifier {
#define DEFINE_APP_BANNER_METRICS_ID(identifier,int) identifier = int,
#define DEFINE_APP_BANNER_METRICS_ID_ENUM_END() };
#include "chrome/browser/android/banners/app_banner_metrics_id_list.h"

};  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_METRICS_IDS_H_
