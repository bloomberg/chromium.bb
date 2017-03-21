// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_REMOVER_HELPER_H_

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "components/reading_list/ios/reading_list_model_observer.h"

namespace ios {
class ChromeBrowserState;
}

class ReadingListModel;
class ReadingListDownloadService;

namespace reading_list {
class ReadingListRemoverHelper : public ReadingListModelObserver {
 public:
  using Callback = base::OnceCallback<void(bool)>;
  ReadingListRemoverHelper(ios::ChromeBrowserState* browser_state);
  ~ReadingListRemoverHelper() override;

  // Removes all Reading list items and call completion with |true| if the
  // cleaning was successful.
  void RemoveAllUserReadingListItemsIOS(Callback completion);

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;

 private:
  Callback completion_;
  ReadingListModel* reading_list_model_;
  ReadingListDownloadService* reading_list_download_service_;
  ScopedObserver<ReadingListModel, ReadingListModelObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListRemoverHelper);
};
}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_REMOVER_HELPER_H_
