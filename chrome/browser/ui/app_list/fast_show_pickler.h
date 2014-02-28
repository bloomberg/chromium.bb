// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_FAST_SHOW_PICKLER_H_
#define CHROME_BROWSER_UI_APP_LIST_FAST_SHOW_PICKLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "ui/app_list/app_list_model.h"

// Functions for pickling/unpickling AppListModel for fast show. Fast show is
// where the app list is put on the screen using data retrieved from a cache
// before the extension system has loaded.
class FastShowPickler {
 public:
  // The version that this pickler understands.
  static const int kVersion;

  // Pickles a subset of the data in |model| that is useful for doing a fast
  // show of the app list.
  static scoped_ptr<Pickle> PickleAppListModelForFastShow(
      app_list::AppListModel* model);

  // Given a Pickle created by PickleAppListModelForFastShow(), this creates an
  // AppListModel that represents it.
  static scoped_ptr<app_list::AppListModel> UnpickleAppListModelForFastShow(
      Pickle* pickle);

  // Copies parts that are needed to show the app list quickly on startup from
  // |src| to |dest|.
  static void CopyOver(
      app_list::AppListModel* src, app_list::AppListModel* dest);

 private:
  // Private static methods allow friend access to AppListItem methods.
  static scoped_ptr<app_list::AppListItem> UnpickleAppListItem(
      PickleIterator* it);
  static bool PickleAppListItem(Pickle* pickle, app_list::AppListItem* item);
  static void CopyOverItem(app_list::AppListItem* src_item,
                           app_list::AppListItem* dest_item);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_FAST_SHOW_PICKLER_H_
