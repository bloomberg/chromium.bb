// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"

namespace {

StringValue* SkColorToCss(SkColor color) {
  return new StringValue(base::StringPrintf("rgb(%d, %d, %d)",
                                            SkColorGetR(color),
                                            SkColorGetG(color),
                                            SkColorGetB(color)));
}

base::StringValue* GetDominantColorCssString(
    scoped_refptr<base::RefCountedMemory> png) {
  color_utils::GridSampler sampler;
  SkColor color =
      color_utils::CalculateKMeanColorOfPNG(png, 100, 665, &sampler);
  return SkColorToCss(color);
}

}  // namespace

// Thin inheritance-dependent trampoline to forward notification of app
// icon loads to the FaviconWebUIHandler. Base class does caching of icons.
class ExtensionIconColorManager : public ExtensionIconManager {
 public:
  explicit ExtensionIconColorManager(FaviconWebUIHandler* handler)
      : ExtensionIconManager(),
        handler_(handler) {}
  virtual ~ExtensionIconColorManager() {}

  virtual void OnImageLoaded(const std::string& extension_id,
                             const gfx::Image& image) OVERRIDE {
    ExtensionIconManager::OnImageLoaded(extension_id, image);
    handler_->NotifyAppIconReady(extension_id);
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
  web_ui()->RegisterMessageCallback("getFaviconDominantColor",
      base::Bind(&FaviconWebUIHandler::HandleGetFaviconDominantColor,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getAppIconDominantColor",
      base::Bind(&FaviconWebUIHandler::HandleGetAppIconDominantColor,
                 base::Unretained(this)));
}

void FaviconWebUIHandler::HandleGetFaviconDominantColor(const ListValue* args) {
  std::string path;
  CHECK(args->GetString(0, &path));
  std::string prefix = "chrome://favicon/size/";
  DCHECK(StartsWithASCII(path, prefix, false)) << "path is " << path;
  size_t slash = path.find("/", prefix.length());
  path = path.substr(slash + 1);

  std::string dom_id;
  CHECK(args->GetString(1, &dom_id));

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()), Profile::EXPLICIT_ACCESS);
  if (!favicon_service || path.empty())
    return;

  GURL url(path);
  // Intercept requests for prepopulated pages.
  for (size_t i = 0; i < arraysize(history::kPrepopulatedPages); i++) {
    if (url.spec() ==
        l10n_util::GetStringUTF8(history::kPrepopulatedPages[i].url_id)) {
      StringValue dom_id_value(dom_id);
      scoped_ptr<StringValue> color(
          SkColorToCss(history::kPrepopulatedPages[i].color));
      web_ui()->CallJavascriptFunction("ntp.setFaviconDominantColor",
                                       dom_id_value, *color);
      return;
    }
  }

  dom_id_map_[id_] = dom_id;
  favicon_service->GetRawFaviconForURL(
      FaviconService::FaviconForURLParams(
          Profile::FromWebUI(web_ui()),
          url,
          history::FAVICON,
          gfx::kFaviconSize),
      ui::SCALE_FACTOR_100P,
      base::Bind(&FaviconWebUIHandler::OnFaviconDataAvailable,
                 base::Unretained(this),
                 id_++),
      &cancelable_task_tracker_);
}

void FaviconWebUIHandler::OnFaviconDataAvailable(
    int id,
    const history::FaviconBitmapResult& bitmap_result) {
  scoped_ptr<StringValue> color_value;

  if (bitmap_result.is_valid())
    color_value.reset(GetDominantColorCssString(bitmap_result.bitmap_data));
  else
    color_value.reset(new StringValue("#919191"));

  StringValue dom_id(dom_id_map_[id]);
  web_ui()->CallJavascriptFunction("ntp.setFaviconDominantColor",
                                   dom_id, *color_value);
  dom_id_map_.erase(id);
}

void FaviconWebUIHandler::HandleGetAppIconDominantColor(
    const ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  ExtensionService* extension_service =
      Profile::FromWebUI(web_ui())->GetExtensionService();
  const extensions::Extension* extension = extension_service->GetExtensionById(
      extension_id, false);
  if (!extension)
    return;
  app_icon_color_manager_->LoadIcon(extension_service->profile(), extension);
}

void FaviconWebUIHandler::NotifyAppIconReady(const std::string& extension_id) {
  const SkBitmap& bitmap = app_icon_color_manager_->GetIcon(extension_id);
  // TODO(estade): would be nice to avoid a round trip through png encoding.
  std::vector<unsigned char> bits;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &bits))
    return;
  scoped_refptr<base::RefCountedStaticMemory> bits_mem(
      new base::RefCountedStaticMemory(&bits.front(), bits.size()));
  scoped_ptr<StringValue> color_value(GetDominantColorCssString(bits_mem));
  StringValue id(extension_id);
  web_ui()->CallJavascriptFunction(
      "ntp.setFaviconDominantColor", id, *color_value);
}
