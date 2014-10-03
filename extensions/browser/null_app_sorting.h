// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NULL_APP_SORTING_H_
#define EXTENSIONS_BROWSER_NULL_APP_SORTING_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/app_sorting.h"

namespace extensions {

// An AppSorting that doesn't provide any ordering.
class NullAppSorting : public AppSorting {
 public:
  NullAppSorting();
  virtual ~NullAppSorting();

  // AppSorting overrides:
  virtual void SetExtensionScopedPrefs(ExtensionScopedPrefs* prefs) override;
  virtual void SetExtensionSyncService(
      ExtensionSyncService* extension_sync_service) override;
  virtual void Initialize(const ExtensionIdList& extension_ids) override;
  virtual void FixNTPOrdinalCollisions() override;
  virtual void EnsureValidOrdinals(
      const std::string& extension_id,
      const syncer::StringOrdinal& suggested_page) override;
  virtual void OnExtensionMoved(
      const std::string& moved_extension_id,
      const std::string& predecessor_extension_id,
      const std::string& successor_extension_id) override;
  virtual syncer::StringOrdinal GetAppLaunchOrdinal(
      const std::string& extension_id) const override;
  virtual void SetAppLaunchOrdinal(
      const std::string& extension_id,
      const syncer::StringOrdinal& new_app_launch_ordinal) override;
  virtual syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  virtual syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  virtual syncer::StringOrdinal CreateFirstAppPageOrdinal() const override;
  virtual syncer::StringOrdinal GetNaturalAppPageOrdinal() const override;
  virtual syncer::StringOrdinal GetPageOrdinal(
      const std::string& extension_id) const override;
  virtual void SetPageOrdinal(
      const std::string& extension_id,
      const syncer::StringOrdinal& new_page_ordinal) override;
  virtual void ClearOrdinals(const std::string& extension_id) override;
  virtual int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const override;
  virtual syncer::StringOrdinal PageIntegerAsStringOrdinal(
      size_t page_index) override;
  virtual void SetExtensionVisible(const std::string& extension_id,
                                   bool visible) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullAppSorting);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NULL_APP_SORTING_H_
