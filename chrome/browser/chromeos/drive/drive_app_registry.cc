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
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

// Webstore URL prefix.
const char kStoreProductUrl[] = "https://chrome.google.com/webstore/";

// Extracts Web store id from its web store URL.
std::string GetWebStoreIdFromUrl(const GURL& url) {
  if (!StartsWithASCII(url.spec(), kStoreProductUrl, false)) {
    LOG(WARNING) << "Unrecognized product URL " << url.spec();
    return std::string();
  }

  base::FilePath path(url.path());
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  DCHECK_LE(2U, components.size());  // Coming from kStoreProductUrl

  // Return the last part of the path
  return components[components.size() - 1];
}

bool SortBySize(const google_apis::InstalledApp::IconList::value_type& a,
                const google_apis::InstalledApp::IconList::value_type& b) {
  return a.first < b.first;
}

}  // namespace

// DriveAppInfo struct implementation.

DriveAppInfo::DriveAppInfo() {
}

DriveAppInfo::DriveAppInfo(
    const std::string& app_id,
    const google_apis::InstalledApp::IconList& app_icons,
    const google_apis::InstalledApp::IconList& document_icons,
    const std::string& web_store_id,
    const std::string& app_name,
    const std::string& object_type,
    bool is_primary_selector)
    : app_id(app_id),
      app_icons(app_icons),
      document_icons(document_icons),
      web_store_id(web_store_id),
      app_name(app_name),
      object_type(object_type),
      is_primary_selector(is_primary_selector) {
}

DriveAppInfo::~DriveAppInfo() {
}

// FileSystem::DriveAppFileSelector struct implementation.

DriveAppRegistry::DriveAppFileSelector::DriveAppFileSelector(
    const GURL& product_link,
    const google_apis::InstalledApp::IconList& app_icons,
    const google_apis::InstalledApp::IconList& document_icons,
    const std::string& object_type,
    const std::string& app_id,
    bool is_primary_selector)
    : product_link(product_link),
      app_icons(app_icons),
      document_icons(document_icons),
      object_type(object_type),
      app_id(app_id),
      is_primary_selector(is_primary_selector) {
}

DriveAppRegistry::DriveAppFileSelector::~DriveAppFileSelector() {
}

// DriveAppRegistry implementation.

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
    const base::FilePath& file_path,
    const std::string& mime_type,
    ScopedVector<DriveAppInfo>* apps) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SelectorAppList result_map;
  if (!file_path.empty()) {
    base::FilePath::StringType extension = file_path.Extension();
    if (extension.size() < 2)
      return;

    extension = extension.substr(1);
    if (!extension.empty())
      FindAppsForSelector(extension, app_extension_map_, &result_map);
  }

  if (!mime_type.empty())
    FindAppsForSelector(mime_type, app_mimetypes_map_, &result_map);

  // Insert found web apps into |apps|, but skip duplicate results.
  std::set<std::string> inserted_app_ids;
  for (SelectorAppList::const_iterator it = result_map.begin();
       it != result_map.end(); ++it) {
    if (inserted_app_ids.find(it->second->app_id) == inserted_app_ids.end()) {
      inserted_app_ids.insert(it->second->app_id);
      apps->push_back(it->second);
    }
  }
}

void DriveAppRegistry::Update() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_updating_)  // There is already an update in progress.
    return;

  is_updating_ = true;

  scheduler_->GetAppList(
      base::Bind(&DriveAppRegistry::UpdateAfterGetAppList,
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

void DriveAppRegistry::UpdateFromAppList(
    const google_apis::AppList& app_list) {
  url_to_name_map_.clear();
  STLDeleteValues(&app_extension_map_);
  STLDeleteValues(&app_mimetypes_map_);
  for (size_t i = 0; i < app_list.items().size(); ++i) {
    const google_apis::AppResource& app = *app_list.items()[i];
    if (app.product_url().is_empty())
      continue;

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
    std::sort(app_icons.begin(), app_icons.end(), SortBySize);
    std::sort(document_icons.begin(), document_icons.end(), SortBySize);

    url_to_name_map_.insert(
        std::make_pair(app.product_url(), app.name()));
    AddAppSelectorList(app.product_url(),
                       app_icons,
                       document_icons,
                       app.object_type(),
                       app.application_id(),
                       true,   // primary
                       app.primary_mimetypes(),
                       &app_mimetypes_map_);
    AddAppSelectorList(app.product_url(),
                       app_icons,
                       document_icons,
                       app.object_type(),
                       app.application_id(),
                       false,   // primary
                       app.secondary_mimetypes(),
                       &app_mimetypes_map_);
    AddAppSelectorList(app.product_url(),
                       app_icons,
                       document_icons,
                       app.object_type(),
                       app.application_id(),
                       true,   // primary
                       app.primary_file_extensions(),
                       &app_extension_map_);
    AddAppSelectorList(app.product_url(),
                       app_icons,
                       document_icons,
                       app.object_type(),
                       app.application_id(),
                       false,   // primary
                       app.secondary_file_extensions(),
                       &app_extension_map_);
  }
}

// static.
void DriveAppRegistry::AddAppSelectorList(
    const GURL& product_link,
    const google_apis::InstalledApp::IconList& app_icons,
    const google_apis::InstalledApp::IconList& document_icons,
    const std::string& object_type,
    const std::string& app_id,
    bool is_primary_selector,
    const ScopedVector<std::string>& selectors,
    DriveAppFileSelectorMap* map) {
  for (ScopedVector<std::string>::const_iterator it = selectors.begin();
       it != selectors.end(); ++it) {
    std::string* value = *it;
    map->insert(std::make_pair(
        *value, new DriveAppFileSelector(product_link,
                                         app_icons,
                                         document_icons,
                                         object_type,
                                         app_id,
                                         is_primary_selector)));
  }
}

void DriveAppRegistry::FindAppsForSelector(
    const std::string& file_selector,
    const DriveAppFileSelectorMap& map,
    SelectorAppList* apps) const {
  for (DriveAppFileSelectorMap::const_iterator it = map.find(file_selector);
       it != map.end() && it->first == file_selector; ++it) {
    const DriveAppFileSelector* web_app = it->second;
    std::map<GURL, std::string>::const_iterator product_iter =
        url_to_name_map_.find(web_app->product_link);
    if (product_iter == url_to_name_map_.end()) {
      NOTREACHED();
      continue;
    }

    std::string web_store_id = GetWebStoreIdFromUrl(web_app->product_link);
    if (web_store_id.empty())
      continue;

    if (apps->find(web_app) != apps->end())
      continue;

    apps->insert(std::make_pair(
        web_app,
        new DriveAppInfo(web_app->app_id,
                         web_app->app_icons,
                         web_app->document_icons,
                         web_store_id,
                         product_iter->second,  // app name
                         web_app->object_type,
                         web_app->is_primary_selector)));
  }
}

namespace util {

GURL FindPreferredIcon(
    const google_apis::InstalledApp::IconList& icons,
    int preferred_size) {
  if (icons.empty())
    return GURL();

  google_apis::InstalledApp::IconList sorted_icons = icons;
  std::sort(sorted_icons.begin(), sorted_icons.end());
  GURL result = sorted_icons.rbegin()->second;
  for (google_apis::InstalledApp::IconList::const_reverse_iterator
           iter = sorted_icons.rbegin();
       iter != sorted_icons.rend() && iter->first >= preferred_size; ++iter) {
    result = iter->second;
  }
  return result;
}

}  // namespace util
}  // namespace drive
