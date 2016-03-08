// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_INTERNAL_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_H_
#define IOS_INTERNAL_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/reading_list/reading_list_model_observer.h"

namespace ios {
class ChromeBrowserState;
}

class ReadingListModel;

// AuthenticationService is the Chrome interface to the iOS shared
// authentication library.
class ShareExtensionService : public KeyedService,
                              public ReadingListModelObserver {
 public:
  explicit ShareExtensionService(ReadingListModel* reading_list_model);
  ~ShareExtensionService() override;

  void Initialize();
  void Shutdown() override;
  void ReadingListModelLoaded(const ReadingListModel* model) override;

 private:
  ReadingListModel* reading_list_model_;  // not owned.

  DISALLOW_COPY_AND_ASSIGN(ShareExtensionService);
};

#endif  // IOS_INTERNAL_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_H_
