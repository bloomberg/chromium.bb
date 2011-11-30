// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_util.h"

#include <vector>

#include "base/base64.h"
#include "base/values.h"
#include "content/browser/disposition_utils.h"
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

WindowOpenDisposition GetDispositionFromClick(const ListValue* args,
                                              int start_index) {
  double button = 0.0;
  bool alt_key = false;
  bool ctrl_key = false;
  bool meta_key = false;
  bool shift_key = false;

  CHECK(args->GetDouble(start_index++, &button));
  CHECK(args->GetBoolean(start_index++, &alt_key));
  CHECK(args->GetBoolean(start_index++, &ctrl_key));
  CHECK(args->GetBoolean(start_index++, &meta_key));
  CHECK(args->GetBoolean(start_index++, &shift_key));
  return disposition_utils::DispositionFromClick(button == 1.0, alt_key,
                                                 ctrl_key, meta_key, shift_key);

}

}  // namespace web_ui_util
