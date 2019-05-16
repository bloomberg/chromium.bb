// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace app_list {

class AppListClient;

// An interface implemented in Ash to handle calls from Chrome.
// These include:
// - When app list data changes in Chrome, it should notifies the UI models and
//   views in Ash to get updated. This can happen while syncing, searching, etc.
// - When Chrome needs real-time UI information from Ash. This can happen while
//   calculating recommended search results based on the app list item order.
// - When app list states in Chrome change that require UI's response. This can
//   happen while installing/uninstalling apps and the app list gets toggled.
class ASH_PUBLIC_EXPORT AppListController {
 public:
  // Gets the instance.
  static AppListController* Get();

  // Sets a client to handle calls from Ash.
  virtual void SetClient(AppListClient* client) = 0;

 protected:
  AppListController();
  virtual ~AppListController();
};

}  // namespace app_list

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
