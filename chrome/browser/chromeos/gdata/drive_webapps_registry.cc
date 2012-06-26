// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {

// WebApp store URL prefix.
const char kStoreProductUrl[] = "https://chrome.google.com/webstore/";

// Extracts Web store id from its web store URL.
std::string GetWebStoreIdFromUrl(const GURL& url) {
  if (!StartsWithASCII(url.spec(), kStoreProductUrl, false)) {
    LOG(WARNING) << "Unrecognized product URL " << url.spec();
    return std::string();
  }

  FilePath path(url.path());
  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);
  DCHECK_LE(2U, components.size());  // Coming from kStoreProductUrl

  // Return the last part of the path
  return components[components.size() - 1];
}

}  // namespace

// DriveWebAppInfo struct implementation.

DriveWebAppInfo::DriveWebAppInfo(const std::string& app_id,
                                 const InstalledApp::IconList& app_icons,
                                 const InstalledApp::IconList& document_icons,
                                 const std::string& web_store_id,
                                 const string16& app_name,
                                 const string16& object_type,
                                 bool is_primary_selector)
    : app_id(app_id),
      app_icons(app_icons),
      document_icons(document_icons),
      web_store_id(web_store_id),
      app_name(app_name),
      object_type(object_type),
      is_primary_selector(is_primary_selector) {
}

DriveWebAppInfo::~DriveWebAppInfo() {
}

// GDataFileSystem::WebAppFileSelector struct implementation.

DriveWebAppsRegistry::WebAppFileSelector::WebAppFileSelector(
    const GURL& product_link,
    const InstalledApp::IconList& app_icons,
    const InstalledApp::IconList& document_icons,
    const string16& object_type,
    const std::string& app_id,
    bool is_primary_selector)
    : product_link(product_link),
      app_icons(app_icons),
      document_icons(document_icons),
      object_type(object_type),
      app_id(app_id),
      is_primary_selector(is_primary_selector) {
}

DriveWebAppsRegistry::WebAppFileSelector::~WebAppFileSelector() {
}

// DriveWebAppsRegistry implementation.

DriveWebAppsRegistry::DriveWebAppsRegistry() {
}

DriveWebAppsRegistry::~DriveWebAppsRegistry() {
  STLDeleteValues(&webapp_extension_map_);
  STLDeleteValues(&webapp_mimetypes_map_);
}

void DriveWebAppsRegistry::GetWebAppsForFile(
    const FilePath& file,
    const std::string& mime_type,
    ScopedVector<DriveWebAppInfo>* apps) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SelectorWebAppList result_map;
  if (!file.empty()) {
    FilePath::StringType extension = file.Extension();
    if (extension.size() < 2)
      return;

    extension = extension.substr(1);
    if (!extension.empty())
      FindWebAppsForSelector(extension, webapp_extension_map_, &result_map);
  }

  if (!mime_type.empty())
    FindWebAppsForSelector(mime_type, webapp_mimetypes_map_, &result_map);

  for (SelectorWebAppList::const_iterator it = result_map.begin();
       it != result_map.end(); ++it) {
    apps->push_back(it->second);
  }
}

std::set<std::string> DriveWebAppsRegistry::GetExtensionsForWebStoreApp(
    const std::string& web_store_id) {
  std::set<std::string> extensions;
  // Iterate over all the registered filename extensions, looking for the given
  // web_store_id.
  for (WebAppFileSelectorMap::iterator iter = webapp_extension_map_.begin();
      iter != webapp_extension_map_.end(); ++iter) {
    std::string id = GetWebStoreIdFromUrl(iter->second->product_link);
    if (id == web_store_id)
      extensions.insert(iter->first);
  }
  return extensions;
}

void DriveWebAppsRegistry::UpdateFromFeed(AccountMetadataFeed* metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  url_to_name_map_.clear();
  STLDeleteValues(&webapp_extension_map_);
  STLDeleteValues(&webapp_mimetypes_map_);
  for (ScopedVector<InstalledApp>::const_iterator it =
           metadata->installed_apps()->begin();
       it != metadata->installed_apps()->end(); ++it) {
    const InstalledApp* app = *it;
    GURL product_url = app->GetProductUrl();
    if (product_url.is_empty())
      continue;

    InstalledApp::IconList app_icons =
        app->GetIconsForCategory(AppIcon::APPLICATION);
    InstalledApp::IconList document_icons =
        app->GetIconsForCategory(AppIcon::DOCUMENT);

    url_to_name_map_.insert(
        std::make_pair(product_url, app->app_name()));
    AddAppSelectorList(product_url,
                       app_icons,
                       document_icons,
                       app->object_type(),
                       app->app_id(),
                       true,   // primary
                       app->primary_mimetypes(),
                       &webapp_mimetypes_map_);
    AddAppSelectorList(product_url,
                       app_icons,
                       document_icons,
                       app->object_type(),
                       app->app_id(),
                       false,   // primary
                       app->secondary_mimetypes(),
                       &webapp_mimetypes_map_);
    AddAppSelectorList(product_url,
                       app_icons,
                       document_icons,
                       app->object_type(),
                       app->app_id(),
                       true,   // primary
                       app->primary_extensions(),
                       &webapp_extension_map_);
    AddAppSelectorList(product_url,
                       app_icons,
                       document_icons,
                       app->object_type(),
                       app->app_id(),
                       false,   // primary
                       app->secondary_extensions(),
                       &webapp_extension_map_);
  }
}

// static.
void DriveWebAppsRegistry::AddAppSelectorList(
    const GURL& product_link,
    const InstalledApp::IconList& app_icons,
    const InstalledApp::IconList& document_icons,
    const string16& object_type,
    const std::string& app_id,
    bool is_primary_selector,
    const ScopedVector<std::string>& selectors,
    WebAppFileSelectorMap* map) {
  for (ScopedVector<std::string>::const_iterator it = selectors.begin();
       it != selectors.end(); ++it) {
    std::string* value = *it;
    map->insert(std::make_pair(
        *value, new WebAppFileSelector(product_link,
                                       app_icons,
                                       document_icons,
                                       object_type,
                                       app_id,
                                       is_primary_selector)));
  }
}

void DriveWebAppsRegistry::FindWebAppsForSelector(
    const std::string& file_selector,
    const WebAppFileSelectorMap& map,
    SelectorWebAppList* apps) {
  for (WebAppFileSelectorMap::const_iterator it = map.find(file_selector);
       it != map.end() && it->first == file_selector; ++it) {
    const WebAppFileSelector* web_app = it->second;
    std::map<GURL, string16>::const_iterator product_iter =
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
        new DriveWebAppInfo(web_app->app_id,
                            web_app->app_icons,
                            web_app->document_icons,
                            web_store_id,
                            product_iter->second,   // app name.
                            web_app->object_type,
                            web_app->is_primary_selector)));
  }
}

}  // namespace gdata
