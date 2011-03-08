// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_util.h"

#include <vector>

#include "base/base64.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace web_ui_util {

std::string GetImageDataUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  std::string str_url;
  str_url.insert(str_url.end(), output.begin(), output.end());

  base::Base64Encode(str_url, &str_url);
  str_url.insert(0, "data:image/png;base64,");
  return str_url;
}

std::string GetImageDataUrlFromResource(int res) {
  // Load resource icon and covert to base64 encoded data url
  RefCountedStaticMemory* icon_data =
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(res);
  if (!icon_data)
    return std::string();
  scoped_refptr<RefCountedMemory> raw_icon(icon_data);
  std::string str_url;
  str_url.insert(str_url.end(),
    raw_icon->front(),
    raw_icon->front() + raw_icon->size());
  base::Base64Encode(str_url, &str_url);
  str_url.insert(0, "data:image/png;base64,");
  return str_url;
}

}  // namespace web_ui_util
