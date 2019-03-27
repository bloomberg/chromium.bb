// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_OPTIONS_H_

#include <iosfwd>

#include "url/gurl.h"

namespace web_app {

enum class InstallSource;
enum class LaunchContainer;

struct InstallOptions {
  InstallOptions(const GURL& url,
                 LaunchContainer launch_container,
                 InstallSource install_source);
  ~InstallOptions();
  InstallOptions(const InstallOptions& other);
  InstallOptions(InstallOptions&& other);
  InstallOptions& operator=(const InstallOptions& other);

  bool operator==(const InstallOptions& other) const;

  GURL url;
  LaunchContainer launch_container;
  InstallSource install_source;

  bool create_shortcuts = true;

  // Whether the app should be reinstalled even if the user has previously
  // uninstalled it.
  bool override_previous_user_uninstall = false;

  // This must only be used by pre-installed default or system apps that are
  // valid PWAs if loading the real service worker is too costly to verify
  // programmatically.
  bool bypass_service_worker_check = false;

  // This should be used for installing all default apps so that good metadata
  // is ensured.
  bool require_manifest = false;

  // Whether the app should be reinstalled even if it is already installed.
  bool always_update = false;
};

std::ostream& operator<<(std::ostream& out,
                         const InstallOptions& install_options);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_OPTIONS_H_
