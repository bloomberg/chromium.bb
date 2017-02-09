// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LAUNCHER_ID_H_
#define CHROME_BROWSER_UI_ASH_APP_LAUNCHER_ID_H_

#include <string>

namespace ash {

// A unique chrome launcher id used to identify a shelf item. This class is a
// wrapper for the chrome launcher identifier. |app_launcher_id_| includes the
// |app_id| and the |launch_id|. The |app_id| is the application id associated
// with a set of windows. The |launch_id| is an id that can be passed to an app
// when launched in order to support multiple shelf items per app. This id is
// used together with the |app_id| to uniquely identify each shelf item that
// has the same |app_id|. The |app_id| must not be empty.
class AppLauncherId {
 public:
  AppLauncherId(const std::string& app_id, const std::string& launch_id);
  // Creates an AppLauncherId with an empty |launch_id|.
  explicit AppLauncherId(const std::string& app_id);
  // Empty constructor for pre-allocating.
  AppLauncherId();
  ~AppLauncherId();

  AppLauncherId(const AppLauncherId& app_launcher_id) = default;
  AppLauncherId(AppLauncherId&& app_launcher_id) = default;
  AppLauncherId& operator=(const AppLauncherId& other) = default;

  const std::string& app_id() const { return app_id_; }
  const std::string& launch_id() const { return launch_id_; }

 private:
  // The application id associated with a set of windows.
  std::string app_id_;
  // An id that can be passed to an app when launched in order to support
  // multiple shelf items per app.
  std::string launch_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_APP_LAUNCHER_ID_H_
