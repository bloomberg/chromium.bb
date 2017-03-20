// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LAUNCH_ID_H_
#define ASH_PUBLIC_CPP_APP_LAUNCH_ID_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// A unique shelf item id composed of an |app_id| and a |launch_id|.
// |app_id| is the non-empty application id associated with a set of windows.
// |launch_id| is passed on app launch, to support multiple shelf items per app.
// As an example, a remote desktop client may want each remote application to
// have its own icon.
class ASH_PUBLIC_EXPORT AppLaunchId {
 public:
  AppLaunchId(const std::string& app_id, const std::string& launch_id);
  // Creates an AppLaunchId with an empty |launch_id|.
  explicit AppLaunchId(const std::string& app_id);
  // Empty constructor for pre-allocating.
  AppLaunchId();
  ~AppLaunchId();

  AppLaunchId(const AppLaunchId& app_launch_id) = default;
  AppLaunchId(AppLaunchId&& app_launch_id) = default;
  AppLaunchId& operator=(const AppLaunchId& other) = default;

  const std::string& app_id() const { return app_id_; }
  const std::string& launch_id() const { return launch_id_; }

 private:
  // The application id associated with a set of windows.
  std::string app_id_;
  // An id passed on app launch, to support multiple shelf items per app.
  std::string launch_id_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LAUNCH_ID_H_
