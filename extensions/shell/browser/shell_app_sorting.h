// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_SORTING_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_SORTING_H_

#include "base/compiler_specific.h"
#include "extensions/browser/app_sorting.h"

namespace extensions {

// A stub AppSorting. Since app_shell only runs a single app we don't need to
// sort them.
class ShellAppSorting : public AppSorting {
 public:
  ShellAppSorting();
  virtual ~ShellAppSorting();

  // AppSorting overrides:
  virtual void SetExtensionScopedPrefs(ExtensionScopedPrefs* prefs) OVERRIDE;
  virtual void SetExtensionSyncService(
      ExtensionSyncService* extension_sync_service) OVERRIDE;
  virtual void Initialize(const ExtensionIdList& extension_ids) OVERRIDE;
  virtual void FixNTPOrdinalCollisions() OVERRIDE;
  virtual void EnsureValidOrdinals(
      const std::string& extension_id,
      const syncer::StringOrdinal& suggested_page) OVERRIDE;
  virtual void OnExtensionMoved(
      const std::string& moved_extension_id,
      const std::string& predecessor_extension_id,
      const std::string& successor_extension_id) OVERRIDE;
  virtual syncer::StringOrdinal GetAppLaunchOrdinal(
      const std::string& extension_id) const OVERRIDE;
  virtual void SetAppLaunchOrdinal(
      const std::string& extension_id,
      const syncer::StringOrdinal& new_app_launch_ordinal) OVERRIDE;
  virtual syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const OVERRIDE;
  virtual syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const OVERRIDE;
  virtual syncer::StringOrdinal CreateFirstAppPageOrdinal() const OVERRIDE;
  virtual syncer::StringOrdinal GetNaturalAppPageOrdinal() const OVERRIDE;
  virtual syncer::StringOrdinal GetPageOrdinal(
      const std::string& extension_id) const OVERRIDE;
  virtual void SetPageOrdinal(
      const std::string& extension_id,
      const syncer::StringOrdinal& new_page_ordinal) OVERRIDE;
  virtual void ClearOrdinals(const std::string& extension_id) OVERRIDE;
  virtual int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const OVERRIDE;
  virtual syncer::StringOrdinal PageIntegerAsStringOrdinal(
      size_t page_index) OVERRIDE;
  virtual void SetExtensionVisible(const std::string& extension_id,
                                   bool visible) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellAppSorting);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_SORTING_H_
