// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/extension_resource.h"
#include "grit/component_extension_resources_map.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "url/gurl.h"

namespace extensions {

ExtensionIconSource::ExtensionIconSource(Profile* profile) : profile_(profile) {
}

struct ExtensionIconSource::ExtensionIconRequest {
  content::URLDataSource::GotDataCallback callback;
  scoped_refptr<const Extension> extension;
  bool grayscale;
  int size;
  ExtensionIconSet::MatchType match;
};

// static
GURL ExtensionIconSource::GetIconURL(const Extension* extension,
                                     int icon_size,
                                     ExtensionIconSet::MatchType match,
                                     bool grayscale,
                                     bool* exists) {
  if (exists) {
    *exists =
        IconsInfo::GetIconURL(extension, icon_size, match) != GURL::EmptyGURL();
  }

  GURL icon_url(base::StringPrintf("%s%s/%d/%d%s",
                                   chrome::kChromeUIExtensionIconURL,
                                   extension->id().c_str(),
                                   icon_size,
                                   match,
                                   grayscale ? "?grayscale=true" : ""));
  CHECK(icon_url.is_valid());
  return icon_url;
}

std::string ExtensionIconSource::GetSource() const {
  return chrome::kChromeUIExtensionIconHost;
}

std::string ExtensionIconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

void ExtensionIconSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_view_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // This is where everything gets started. First, parse the request and make
  // the request data available for later.
  static int next_id = 0;
  if (!ParseData(path, ++next_id, callback)) {
    // If the request data cannot be parsed, we will request anyway a default
    // icon (not resized or desaturated).
    SetData(next_id, callback, NULL, false, -1, ExtensionIconSet::MATCH_BIGGER);
  }

  LoadExtensionImage(next_id);
}

ExtensionIconSource::~ExtensionIconSource() {
  // Clean up all the temporary data we're holding for requests.
  STLDeleteValues(&request_map_);
}

void ExtensionIconSource::LoadExtensionImage(int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  ImageLoader::Get(profile_)->LoadExtensionIconAsync(
      request->extension,
      request->size,
      request->match,
      request->grayscale,
      base::Bind(&ExtensionIconSource::OnIconLoaded, AsWeakPtr(), request_id));
}

void ExtensionIconSource::OnIconLoaded(int request_id, const gfx::Image& image)
{
  ExtensionIconRequest* request = GetData(request_id);
  request->callback.Run(ImageLoader::BitmapToMemory(image.ToSkBitmap()).get());
  ClearData(request_id);
}

bool ExtensionIconSource::ParseData(
    const std::string& path,
    int request_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // Extract the parameters from the path by lower casing and splitting.
  std::string path_lower = StringToLowerASCII(path);
  std::vector<std::string> path_parts;

  base::SplitString(path_lower, '/', &path_parts);
  if (path_lower.empty() || path_parts.size() < 3)
    return false;

  std::string size_param = path_parts.at(1);
  std::string match_param = path_parts.at(2);
  match_param = match_param.substr(0, match_param.find('?'));

  int size;
  if (!base::StringToInt(size_param, &size))
    return false;
  if (size <= 0 || size > extension_misc::EXTENSION_ICON_GIGANTOR)
    return false;

  ExtensionIconSet::MatchType match_type;
  int match_num;
  if (!base::StringToInt(match_param, &match_num))
    return false;
  match_type = static_cast<ExtensionIconSet::MatchType>(match_num);
  if (!(match_type == ExtensionIconSet::MATCH_EXACTLY ||
        match_type == ExtensionIconSet::MATCH_SMALLER ||
        match_type == ExtensionIconSet::MATCH_BIGGER))
    match_type = ExtensionIconSet::MATCH_EXACTLY;

  std::string extension_id = path_parts.at(0);
  const Extension* extension = ExtensionSystem::Get(profile_)->
      extension_service()->GetInstalledExtension(extension_id);
  if (!extension)
    return false;

  bool grayscale = path_lower.find("grayscale=true") != std::string::npos;

  SetData(request_id, callback, extension, grayscale, size, match_type);

  return true;
}

void ExtensionIconSource::SetData(
    int request_id,
    const content::URLDataSource::GotDataCallback& callback,
    const Extension* extension,
    bool grayscale,
    int size,
    ExtensionIconSet::MatchType match) {
  ExtensionIconRequest* request = new ExtensionIconRequest();
  request->callback = callback;
  request->extension = extension;
  request->grayscale = grayscale;
  request->size = size;
  request->match = match;
  request_map_[request_id] = request;
}

ExtensionIconSource::ExtensionIconRequest* ExtensionIconSource::GetData(
    int request_id) {
  return request_map_[request_id];
}

void ExtensionIconSource::ClearData(int request_id) {
  std::map<int, ExtensionIconRequest*>::iterator i =
      request_map_.find(request_id);
  if (i == request_map_.end())
    return;

  delete i->second;
  request_map_.erase(i);
}

}  // namespace extensions
