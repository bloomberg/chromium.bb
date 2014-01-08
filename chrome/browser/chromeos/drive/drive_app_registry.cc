// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_app_registry.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"

using content::BrowserThread;

namespace drive {

DriveAppInfo::DriveAppInfo() {
}

DriveAppInfo::DriveAppInfo(
    const std::string& app_id,
    const google_apis::InstalledApp::IconList& app_icons,
    const google_apis::InstalledApp::IconList& document_icons,
    const std::string& app_name,
    const GURL& create_url)
    : app_id(app_id),
      app_icons(app_icons),
      document_icons(document_icons),
      app_name(app_name),
      create_url(create_url) {
}

DriveAppInfo::~DriveAppInfo() {
}

DriveAppRegistry::DriveAppRegistry(JobScheduler* scheduler)
    : scheduler_(scheduler),
      is_updating_(false),
      weak_ptr_factory_(this) {
}

DriveAppRegistry::~DriveAppRegistry() {
  STLDeleteValues(&app_extension_map_);
  STLDeleteValues(&app_mimetypes_map_);
}

void DriveAppRegistry::GetAppsForFile(
    const base::FilePath::StringType& file_extension,
    const std::string& mime_type,
    ScopedVector<DriveAppInfo>* apps) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<DriveAppInfo*> matched_apps;
  if (!file_extension.empty()) {
    const base::FilePath::StringType without_dot = file_extension.substr(1);
    FindAppsForSelector(without_dot, app_extension_map_, &matched_apps);
  }
  if (!mime_type.empty())
    FindAppsForSelector(mime_type, app_mimetypes_map_, &matched_apps);

  // Insert found Drive apps into |apps|, but skip duplicate results.
  std::set<std::string> inserted_app_ids;
  for (size_t i = 0; i < matched_apps.size(); ++i) {
    if (inserted_app_ids.count(matched_apps[i]->app_id) == 0) {
      inserted_app_ids.insert(matched_apps[i]->app_id);
      apps->push_back(new DriveAppInfo(*matched_apps[i]));
    }
  }
}

void DriveAppRegistry::Update() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_updating_)  // There is already an update in progress.
    return;
  is_updating_ = true;

  scheduler_->GetAppList(base::Bind(&DriveAppRegistry::UpdateAfterGetAppList,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void DriveAppRegistry::UpdateAfterGetAppList(
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(is_updating_);
  is_updating_ = false;

  FileError error = GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    // Failed to fetch the data from the server. We can do nothing here.
    return;
  }

  DCHECK(app_list);
  UpdateFromAppList(*app_list);
}

void DriveAppRegistry::UpdateFromAppList(const google_apis::AppList& app_list) {
  STLDeleteValues(&app_extension_map_);
  STLDeleteValues(&app_mimetypes_map_);

  for (size_t i = 0; i < app_list.items().size(); ++i) {
    const google_apis::AppResource& app = *app_list.items()[i];

    google_apis::InstalledApp::IconList app_icons;
    google_apis::InstalledApp::IconList document_icons;
    for (size_t j = 0; j < app.icons().size(); ++j) {
      const google_apis::DriveAppIcon& icon = *app.icons()[j];
      if (icon.icon_url().is_empty())
        continue;
      if (icon.category() == google_apis::DriveAppIcon::APPLICATION)
        app_icons.push_back(std::make_pair(icon.icon_side_length(),
                                           icon.icon_url()));
      if (icon.category() == google_apis::DriveAppIcon::DOCUMENT)
        document_icons.push_back(std::make_pair(icon.icon_side_length(),
                                                icon.icon_url()));
    }

    AddAppSelectorList(app.name(),
                       app_icons,
                       document_icons,
                       app.application_id(),
                       app.create_url(),
                       app.primary_mimetypes(),
                       &app_mimetypes_map_);
    AddAppSelectorList(app.name(),
                       app_icons,
                       document_icons,
                       app.application_id(),
                       app.create_url(),
                       app.secondary_mimetypes(),
                       &app_mimetypes_map_);
    AddAppSelectorList(app.name(),
                       app_icons,
                       document_icons,
                       app.application_id(),
                       app.create_url(),
                       app.primary_file_extensions(),
                       &app_extension_map_);
    AddAppSelectorList(app.name(),
                       app_icons,
                       document_icons,
                       app.application_id(),
                       app.create_url(),
                       app.secondary_file_extensions(),
                       &app_extension_map_);
  }
}

// static.
void DriveAppRegistry::AddAppSelectorList(
    const std::string& app_name,
    const google_apis::InstalledApp::IconList& app_icons,
    const google_apis::InstalledApp::IconList& document_icons,
    const std::string& app_id,
    const GURL& create_url,
    const ScopedVector<std::string>& selectors,
    DriveAppFileSelectorMap* map) {
  for (ScopedVector<std::string>::const_iterator it = selectors.begin();
       it != selectors.end(); ++it) {
    std::string* value = *it;
    map->insert(std::make_pair(
        *value, new DriveAppInfo(app_id,
                                 app_icons,
                                 document_icons,
                                 app_name,
                                 create_url)));
  }
}

void DriveAppRegistry::FindAppsForSelector(
    const std::string& file_selector,
    const DriveAppFileSelectorMap& map,
    std::vector<DriveAppInfo*>* matched_apps) const {
  for (DriveAppFileSelectorMap::const_iterator it = map.find(file_selector);
       it != map.end() && it->first == file_selector; ++it) {
    matched_apps->push_back(it->second);
  }
}

namespace util {

GURL FindPreferredIcon(const google_apis::InstalledApp::IconList& icons,
                       int preferred_size) {
  if (icons.empty())
    return GURL();

  google_apis::InstalledApp::IconList sorted_icons = icons;
  std::sort(sorted_icons.rbegin(), sorted_icons.rend());

  // Go forward while the size is larger or equal to preferred_size.
  size_t i = 1;
  while (i < sorted_icons.size() && sorted_icons[i].first >= preferred_size)
    ++i;
  return sorted_icons[i - 1].second;
}

}  // namespace util
}  // namespace drive
