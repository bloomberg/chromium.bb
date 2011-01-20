// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_util.h"

#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/values.h"
#include "gfx/codec/png_codec.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_ui_util {

std::string GetJsonResponseFromFirstArgumentInList(const ListValue* args) {
  return GetJsonResponseFromArgumentList(args, 0);
}

std::string GetJsonResponseFromArgumentList(const ListValue* args,
                                            size_t list_index) {
  std::string result;
  if (args->GetSize() <= list_index) {
    NOTREACHED();
    return result;
  }

  Value* value = NULL;
  if (args->Get(list_index, &value))
    value->GetAsString(&result);
  else
    NOTREACHED();

  return result;
}

std::string GetImageDataUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  std::string str_url;
  std::copy(output.begin(), output.end(),
            std::back_inserter(str_url));
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
  std::copy(raw_icon->front(), raw_icon->front() + raw_icon->size(),
            std::back_inserter(str_url));
  base::Base64Encode(str_url, &str_url);
  str_url.insert(0, "data:image/png;base64,");
  return str_url;
}

}  // end of namespace dom_ui_util
