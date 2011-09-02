// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"

#include "base/callback.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"

// Thin inheritance-dependent trampoline to forward notification of app
// icon loads to the FaviconWebUIHandler. Base class does caching of icons.
class ExtensionIconColorManager : public ExtensionIconManager {
 public:
  explicit ExtensionIconColorManager(FaviconWebUIHandler* handler)
      : ExtensionIconManager(),
        handler_(handler) {}
  virtual ~ExtensionIconColorManager() {}

  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE {
    ExtensionIconManager::OnImageLoaded(image, resource, index);
    handler_->NotifyAppIconReady(resource.extension_id());
  }

 private:
  FaviconWebUIHandler* handler_;
};

FaviconWebUIHandler::FaviconWebUIHandler()
    : app_icon_color_manager_(new ExtensionIconColorManager(this)) {
}

FaviconWebUIHandler::~FaviconWebUIHandler() {
}

void FaviconWebUIHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getFaviconDominantColor",
      NewCallback(this, &FaviconWebUIHandler::HandleGetFaviconDominantColor));
  web_ui_->RegisterMessageCallback("getAppIconDominantColor",
      NewCallback(this,
                  &FaviconWebUIHandler::HandleGetAppIconDominantColor));
}

void FaviconWebUIHandler::HandleGetFaviconDominantColor(const ListValue* args) {
  std::string path;
  CHECK(args->GetString(0, &path));
  DCHECK(StartsWithASCII(path, "chrome://favicon/size/16/", false)) <<
      "path is " << path;
  path = path.substr(arraysize("chrome://favicon/size/16/") - 1);

  double id;
  CHECK(args->GetDouble(1, &id));

  std::string callback_name;
  CHECK(args->GetString(2, &callback_name));
  callbacks_map_[static_cast<int>(id)] = callback_name;

  FaviconService* favicon_service =
      Profile::FromWebUI(web_ui_)->GetFaviconService(Profile::EXPLICIT_ACCESS);
  if (!favicon_service || path.empty())
    return;

  FaviconService::Handle handle = favicon_service->GetFaviconForURL(
      GURL(path),
      history::FAVICON,
      &consumer_,
      NewCallback(this, &FaviconWebUIHandler::OnFaviconDataAvailable));
  consumer_.SetClientData(favicon_service, handle, static_cast<int>(id));
}

void FaviconWebUIHandler::OnFaviconDataAvailable(
    FaviconService::Handle request_handle,
    history::FaviconData favicon) {
  FaviconService* favicon_service =
      Profile::FromWebUI(web_ui_)->GetFaviconService(Profile::EXPLICIT_ACCESS);
  int id = consumer_.GetClientData(favicon_service, request_handle);
  base::FundamentalValue id_value(id);
  scoped_ptr<StringValue> color_value;

  if (favicon.is_valid()) {
    // TODO(estade): cache the response
    color_value.reset(GetDominantColor(favicon.image_data));
  } else {
    color_value.reset(new StringValue("#919191"));
  }

  web_ui_->CallJavascriptFunction(callbacks_map_[id], id_value, *color_value);
  callbacks_map_.erase(id);
}

StringValue* FaviconWebUIHandler::GetDominantColor(
    scoped_refptr<RefCountedMemory> png) {
  color_utils::GridSampler sampler;
  SkColor color = color_utils::CalculateKMeanColorOfPNG(png, 100, 665, sampler);
  std::string css_color = base::StringPrintf("rgb(%d, %d, %d)",
                                             SkColorGetR(color),
                                             SkColorGetG(color),
                                             SkColorGetB(color));
  return new StringValue(css_color);
}

void FaviconWebUIHandler::HandleGetAppIconDominantColor(
    const ListValue* args) {
  std::string path;
  CHECK(args->GetString(0, &path));
  GURL gurl(path);
  DCHECK(StartsWithASCII(path, chrome::kChromeUIExtensionIconURL, false)) <<
      "path is " << path;

  std::string callback_name;
  CHECK(args->GetString(1, &callback_name));

  // See ExtensionIconSource::ParseData
  std::string path_lower = StringToLowerASCII(gurl.path());
  std::vector<std::string> path_parts;
  base::SplitString(path_lower, '/', &path_parts);
  CHECK(path_parts.size() > 1);
  std::string extension_id = path_parts.at(1);
  app_callbacks_map_[extension_id] = callback_name;

  ExtensionService* extension_service =
      Profile::FromWebUI(web_ui_)->GetExtensionService();
  const Extension* extension = extension_service->GetExtensionById(
      extension_id, false);
  CHECK(extension);
  app_icon_color_manager_->LoadIcon(extension);
}

void FaviconWebUIHandler::NotifyAppIconReady(const std::string& extension_id) {
  const SkBitmap& bitmap = app_icon_color_manager_->GetIcon(extension_id);
  // TODO(gbillock): cache this? Probably call into it from the manager and
  // cache the color there. Then call back to this method with the ext id
  // and the color.
  std::vector<unsigned char> bits;
  CHECK(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &bits));
  scoped_refptr<RefCountedStaticMemory> bits_mem(
      new RefCountedStaticMemory(&bits.front(), bits.size()));
  scoped_ptr<StringValue> color_value(GetDominantColor(bits_mem));
  StringValue extension_id_value(extension_id);
  web_ui_->CallJavascriptFunction(
      app_callbacks_map_[extension_id], extension_id_value, *color_value);
  app_callbacks_map_.erase(extension_id);
}
