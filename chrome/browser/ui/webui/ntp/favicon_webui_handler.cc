// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"

#include "base/bind.h"
#include "base/callback_old.h"
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
    : id_(0),
      app_icon_color_manager_(new ExtensionIconColorManager(this)) {
}

FaviconWebUIHandler::~FaviconWebUIHandler() {
}

void FaviconWebUIHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getFaviconDominantColor",
      base::Bind(&FaviconWebUIHandler::HandleGetFaviconDominantColor,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getAppIconDominantColor",
      base::Bind(&FaviconWebUIHandler::HandleGetAppIconDominantColor,
                 base::Unretained(this)));
}

void FaviconWebUIHandler::HandleGetFaviconDominantColor(const ListValue* args) {
  std::string path;
  CHECK(args->GetString(0, &path));
  DCHECK(StartsWithASCII(path, "chrome://favicon/size/16/", false)) <<
      "path is " << path;
  path = path.substr(arraysize("chrome://favicon/size/16/") - 1);

  std::string dom_id;
  CHECK(args->GetString(1, &dom_id));
  dom_id_map_[id_] = dom_id;

  FaviconService* favicon_service =
      Profile::FromWebUI(web_ui_)->GetFaviconService(Profile::EXPLICIT_ACCESS);
  if (!favicon_service || path.empty())
    return;

  FaviconService::Handle handle = favicon_service->GetFaviconForURL(
      GURL(path),
      history::FAVICON,
      &consumer_,
      NewCallback(this, &FaviconWebUIHandler::OnFaviconDataAvailable));
  consumer_.SetClientData(favicon_service, handle, id_++);
}

void FaviconWebUIHandler::OnFaviconDataAvailable(
    FaviconService::Handle request_handle,
    history::FaviconData favicon) {
  FaviconService* favicon_service =
      Profile::FromWebUI(web_ui_)->GetFaviconService(Profile::EXPLICIT_ACCESS);
  int id = consumer_.GetClientData(favicon_service, request_handle);
  scoped_ptr<StringValue> color_value;

  if (favicon.is_valid())
    color_value.reset(GetDominantColorCssString(favicon.image_data));
  else
    color_value.reset(new StringValue("#919191"));

  StringValue dom_id(dom_id_map_[id]);
  web_ui_->CallJavascriptFunction("ntp4.setStripeColor", dom_id, *color_value);
  dom_id_map_.erase(id);
}

StringValue* FaviconWebUIHandler::GetDominantColorCssString(
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
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  ExtensionService* extension_service =
      Profile::FromWebUI(web_ui_)->GetExtensionService();
  const Extension* extension = extension_service->GetExtensionById(
      extension_id, false);
  if (!extension)
    return;
  app_icon_color_manager_->LoadIcon(extension);
}

void FaviconWebUIHandler::NotifyAppIconReady(const std::string& extension_id) {
  const SkBitmap& bitmap = app_icon_color_manager_->GetIcon(extension_id);
  // TODO(estade): would be nice to avoid a round trip through png encoding.
  std::vector<unsigned char> bits;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &bits))
    return;
  scoped_refptr<RefCountedStaticMemory> bits_mem(
      new RefCountedStaticMemory(&bits.front(), bits.size()));
  scoped_ptr<StringValue> color_value(GetDominantColorCssString(bits_mem));
  StringValue id(extension_id);
  web_ui_->CallJavascriptFunction(
      "ntp4.setStripeColor", id, *color_value);
}
