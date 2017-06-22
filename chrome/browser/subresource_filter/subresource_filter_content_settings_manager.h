// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/history/core/browser/history_service_observer.h"

class ContentSettingsPattern;
class GURL;
class HostContentSettingsMap;
class Profile;

namespace base {
class DictionaryValue;
}  // namespace base

namespace history {
class HistoryService;
}  // namespace history

// This class contains helpers to get/set content and website settings related
// to subresource filtering. It also observes content setting changes for
// metrics collection.
class SubresourceFilterContentSettingsManager
    : public content_settings::Observer,
      public history::HistoryServiceObserver {
 public:
  explicit SubresourceFilterContentSettingsManager(Profile* profile);
  ~SubresourceFilterContentSettingsManager() override;

  ContentSetting GetSitePermission(const GURL& url) const;

  // Only called via direct user action on via the subresource filter UI. Sets
  // the content setting to turn off the subresource filter.
  void WhitelistSite(const GURL& url);

  // Public for testing.
  std::unique_ptr<base::DictionaryValue> GetSiteMetadata(const GURL& url) const;

  // Specific logic for more intelligent UI.
  void OnDidShowUI(const GURL& url);
  bool ShouldShowUIForSite(const GURL& url) const;
  bool should_use_smart_ui() const { return should_use_smart_ui_; }
  void set_should_use_smart_ui_for_testing(bool should_use_smart_ui) {
    should_use_smart_ui_ = should_use_smart_ui;
  }

  void ClearSiteMetadata(const GURL& url);

  void set_clock_for_testing(std::unique_ptr<base::Clock> tick_clock) {
    clock_ = std::move(tick_clock);
  }

  // Time before showing the UI again on a domain.
  // TODO(csharrison): Consider setting this via a finch param.
  static constexpr base::TimeDelta kDelayBeforeShowingInfobarAgain =
      base::TimeDelta::FromHours(24);

 private:
  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  void SetSiteMetadata(const GURL& url,
                       std::unique_ptr<base::DictionaryValue> dict);

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_;

  HostContentSettingsMap* settings_map_;

  // Used only for metrics so we don't report global changes which just keep
  // the same value.
  ContentSetting cached_global_setting_for_metrics_;

  // A clock is injected into this class so tests can set arbitrary timestamps
  // in website settings.
  std::unique_ptr<base::Clock> clock_;

  // Used internally so the class ignores changes to the settings that are not
  // user initiated through the settings UI.
  bool ignore_settings_changes_ = false;

  bool should_use_smart_ui_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentSettingsManager);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_
