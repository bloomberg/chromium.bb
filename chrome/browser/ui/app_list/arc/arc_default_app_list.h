// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_DEFAULT_APP_LIST_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_DEFAULT_APP_LIST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}

// Contains map of default pre-installed apps and packages.
class ArcDefaultAppList {
 public:
  class Delegate {
   public:
    virtual void OnDefaultAppsReady() = 0;

   protected:
    virtual ~Delegate() {}
  };

  struct AppInfo {
    AppInfo(const std::string& name,
                   const std::string& package_name,
                   const std::string& activity,
                   bool oem,
                   const base::FilePath app_path);
    ~AppInfo();

    std::string name;
    std::string package_name;
    std::string activity;
    bool oem;
    base::FilePath app_path;  // App folder that contains pre-installed icons.
  };

  // Defines App id to default AppInfo mapping.
  using AppInfoMap = std::map<std::string, std::unique_ptr<AppInfo>>;

  ArcDefaultAppList(Delegate* delegate, content::BrowserContext* context);
  ~ArcDefaultAppList();

  static void UseTestAppsDirectory();

  // Returns default app info if it is found in defaults and its package is not
  // marked as uninstalled.
  const AppInfo* GetApp(const std::string& app_id) const;
  // Returns true if app is found in defaults and its package is not marked as
  // uninstalled.
  bool HasApp(const std::string& app_id) const;
  // Returns true if package exists in default packages list. Note it may be
  // marked as uninstalled.
  bool HasPackage(const std::string& package_name) const;
  // Sets uninstalled flag for default package if it exists in default packages
  // list.
  void MaybeMarkPackageUninstalled(const std::string& package_name,
                                   bool uninstalled);

  const AppInfoMap& app_map() const { return apps_; }

 private:
  // Defines mapping package name to uninstalled state.
  using PacakageMap = std::map<std::string, bool>;

  // Called when default apps are read.
  void OnAppsReady(std::unique_ptr<AppInfoMap> apps);

  // Unowned pointer.
  Delegate* const delegate_;
  content::BrowserContext* const context_;

  AppInfoMap apps_;
  PacakageMap packages_;

  base::WeakPtrFactory<ArcDefaultAppList> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcDefaultAppList);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_DEFAULT_APP_LIST_H_
