// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/component_extension_resources_map.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/image_decoder.h"

namespace {

scoped_refptr<base::RefCountedMemory> BitmapToMemory(const SkBitmap* image) {
  base::RefCountedBytes* image_bytes = new base::RefCountedBytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(*image, false, &image_bytes->data());
  return image_bytes;
}

SkBitmap DesaturateImage(const SkBitmap* image) {
  color_utils::HSL shift = {-1, 0, 0.6};
  return SkBitmapOperations::CreateHSLShiftedBitmap(*image, shift);
}

SkBitmap* ToBitmap(const unsigned char* data, size_t size) {
  webkit_glue::ImageDecoder decoder;
  SkBitmap* decoded = new SkBitmap();
  *decoded = decoder.Decode(data, size);
  return decoded;
}

}  // namespace

ExtensionIconSource::ExtensionIconSource(Profile* profile)
    : DataSource(chrome::kChromeUIExtensionIconHost, MessageLoop::current()),
      profile_(profile),
      next_tracker_id_(0) {
  tracker_.reset(new ImageLoadingTracker(this));
}

struct ExtensionIconSource::ExtensionIconRequest {
  int request_id;
  const Extension* extension;
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
  if (exists)
    *exists = true;
  if (exists && extension->GetIconURL(icon_size, match) == GURL())
    *exists = false;

  GURL icon_url(base::StringPrintf("%s%s/%d/%d%s",
                                   chrome::kChromeUIExtensionIconURL,
                                   extension->id().c_str(),
                                   icon_size,
                                   match,
                                   grayscale ? "?grayscale=true" : ""));
  CHECK(icon_url.is_valid());
  return icon_url;
}

// static
SkBitmap* ExtensionIconSource::LoadImageByResourceId(int resource_id) {
  std::string contents = ResourceBundle::GetSharedInstance()
      .GetRawDataResource(resource_id,
                          ui::SCALE_FACTOR_100P).as_string();

  // Convert and return it.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(contents.data());
  return ToBitmap(data, contents.length());
}

std::string ExtensionIconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

void ExtensionIconSource::StartDataRequest(const std::string& path,
                                           bool is_incognito,
                                           int request_id) {
  // This is where everything gets started. First, parse the request and make
  // the request data available for later.
  if (!ParseData(path, request_id)) {
    SendDefaultResponse(request_id);
    return;
  }

  ExtensionIconRequest* request = GetData(request_id);
  ExtensionResource icon =
      request->extension->GetIconResource(request->size, request->match);

  if (icon.relative_path().empty()) {
    LoadIconFailed(request_id);
  } else {
    LoadExtensionImage(icon, request_id);
  }
}

ExtensionIconSource::~ExtensionIconSource() {
  // Clean up all the temporary data we're holding for requests.
  STLDeleteValues(&request_map_);
}

const SkBitmap* ExtensionIconSource::GetWebStoreImage() {
  if (!web_store_icon_data_.get())
    web_store_icon_data_.reset(LoadImageByResourceId(IDR_WEBSTORE_ICON));

  return web_store_icon_data_.get();
}

const SkBitmap* ExtensionIconSource::GetDefaultAppImage() {
  if (!default_app_data_.get())
    default_app_data_.reset(LoadImageByResourceId(IDR_APP_DEFAULT_ICON));

  return default_app_data_.get();
}

const SkBitmap* ExtensionIconSource::GetDefaultExtensionImage() {
  if (!default_extension_data_.get()) {
    default_extension_data_.reset(
        LoadImageByResourceId(IDR_EXTENSION_DEFAULT_ICON));
  }

  return default_extension_data_.get();
}

void ExtensionIconSource::FinalizeImage(const SkBitmap* image,
                                        int request_id) {
  SkBitmap bitmap;
  if (GetData(request_id)->grayscale)
    bitmap = DesaturateImage(image);
  else
    bitmap = *image;

  ClearData(request_id);
  SendResponse(request_id, BitmapToMemory(&bitmap));
}

void ExtensionIconSource::LoadDefaultImage(int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  const SkBitmap* default_image = NULL;

  if (request->extension->id() == extension_misc::kWebStoreAppId)
    default_image = GetWebStoreImage();
  else if (request->extension->is_app())
    default_image = GetDefaultAppImage();
  else
    default_image = GetDefaultExtensionImage();

  SkBitmap resized_image(skia::ImageOperations::Resize(
      *default_image, skia::ImageOperations::RESIZE_LANCZOS3,
      request->size, request->size));

  // There are cases where Resize returns an empty bitmap, for example if you
  // ask for an image too large. In this case it is better to return the default
  // image than returning nothing at all.
  if (resized_image.empty())
    resized_image = *default_image;

  FinalizeImage(&resized_image, request_id);
}

void ExtensionIconSource::LoadExtensionImage(const ExtensionResource& icon,
                                             int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  tracker_map_[next_tracker_id_++] = request_id;
  tracker_->LoadImage(request->extension,
                      icon,
                      gfx::Size(request->size, request->size),
                      ImageLoadingTracker::DONT_CACHE);
}

void ExtensionIconSource::LoadFaviconImage(int request_id) {
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  // Fall back to the default icons if the service isn't available.
  if (favicon_service == NULL) {
    LoadDefaultImage(request_id);
    return;
  }

  GURL favicon_url = GetData(request_id)->extension->GetFullLaunchURL();
  FaviconService::Handle handle = favicon_service->GetFaviconForURL(
      favicon_url,
      history::FAVICON,
      &cancelable_consumer_,
      base::Bind(&ExtensionIconSource::OnFaviconDataAvailable,
                 base::Unretained(this)));
  cancelable_consumer_.SetClientData(favicon_service, handle, request_id);
}

void ExtensionIconSource::OnFaviconDataAvailable(
    FaviconService::Handle request_handle,
    history::FaviconData favicon) {
  int request_id = cancelable_consumer_.GetClientData(
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS), request_handle);
  ExtensionIconRequest* request = GetData(request_id);

  // Fallback to the default icon if there wasn't a favicon.
  if (!favicon.is_valid()) {
    LoadDefaultImage(request_id);
    return;
  }

  if (!request->grayscale) {
    // If we don't need a grayscale image, then we can bypass FinalizeImage
    // to avoid unnecessary conversions.
    ClearData(request_id);
    SendResponse(request_id, favicon.image_data);
  } else {
    FinalizeImage(ToBitmap(favicon.image_data->front(),
                           favicon.image_data->size()), request_id);
  }
}

void ExtensionIconSource::OnImageLoaded(const gfx::Image& image,
                                        const std::string& extension_id,
                                        int index) {
  int request_id = tracker_map_[index];
  tracker_map_.erase(tracker_map_.find(index));

  if (image.IsEmpty())
    LoadIconFailed(request_id);
  else
    FinalizeImage(image.ToSkBitmap(), request_id);
}

void ExtensionIconSource::LoadIconFailed(int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  ExtensionResource icon =
      request->extension->GetIconResource(request->size, request->match);

  if (request->size == ExtensionIconSet::EXTENSION_ICON_BITTY)
    LoadFaviconImage(request_id);
  else
    LoadDefaultImage(request_id);
}

bool ExtensionIconSource::ParseData(const std::string& path,
                                    int request_id) {
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
  if (size <= 0)
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
  const Extension* extension =
      profile_->GetExtensionService()->GetInstalledExtension(extension_id);
  if (!extension)
    return false;

  bool grayscale = path_lower.find("grayscale=true") != std::string::npos;

  SetData(request_id, extension, grayscale, size, match_type);

  return true;
}

void ExtensionIconSource::SendDefaultResponse(int request_id) {
  // We send back the default application icon (not resized or desaturated)
  // as the default response, like when there is no data.
  ClearData(request_id);
  SendResponse(request_id, BitmapToMemory(GetDefaultAppImage()));
}

void ExtensionIconSource::SetData(int request_id,
                                  const Extension* extension,
                                  bool grayscale,
                                  int size,
                                  ExtensionIconSet::MatchType match) {
  ExtensionIconRequest* request = new ExtensionIconRequest();
  request->request_id = request_id;
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
