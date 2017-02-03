// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_BUILDER_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_BUILDER_H_

@class ShareToData;
@class Tab;

namespace activity_services {

// Returns a ShareToData object using data from |tab|. |tab| must not be nil.
// Function may return nil.
ShareToData* ShareToDataForTab(Tab* tab);

}  // namespace activity_services

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_BUILDER_H_
