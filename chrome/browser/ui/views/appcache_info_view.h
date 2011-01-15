// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPCACHE_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPCACHE_INFO_VIEW_H_
#pragma once

#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/ui/views/generic_info_view.h"

// AppCacheInfoView
// Displays a tabular grid of AppCache information.
class AppCacheInfoView : public GenericInfoView {
 public:
  AppCacheInfoView();
  void SetAppCacheInfo(const appcache::AppCacheInfo* info);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppCacheInfoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPCACHE_INFO_VIEW_H_

