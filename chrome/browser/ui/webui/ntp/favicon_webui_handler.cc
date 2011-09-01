// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/favicon_webui_handler.h"

#include "base/callback.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"

FaviconWebUIHandler::FaviconWebUIHandler() {
}

FaviconWebUIHandler::~FaviconWebUIHandler() {
}

void FaviconWebUIHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getFaviconDominantColor",
      NewCallback(this, &FaviconWebUIHandler::HandleGetFaviconDominantColor));
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
    color_utils::GridSampler sampler;
    SkColor color =
        color_utils::CalculateKMeanColorOfPNG(favicon.image_data, 100, 665,
                                              sampler);
    std::string css_color = base::StringPrintf("rgb(%d, %d, %d)",
                                               SkColorGetR(color),
                                               SkColorGetG(color),
                                               SkColorGetB(color));
    color_value.reset(new StringValue(css_color));
  } else {
    color_value.reset(new StringValue("#919191"));
  }

  web_ui_->CallJavascriptFunction(callbacks_map_[id], id_value, *color_value);
  callbacks_map_.erase(id);
}
