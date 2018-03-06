// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ICON_DECODE_REQUEST_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ICON_DECODE_REQUEST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/image_decoder.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace app_list {

class IconDecodeRequest : public ImageDecoder::ImageRequest {
 public:
  using SetIconCallback = base::OnceCallback<void(const gfx::ImageSkia& icon)>;

  explicit IconDecodeRequest(SetIconCallback set_icon_callback);
  ~IconDecodeRequest() override;

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

 private:
  SetIconCallback set_icon_callback_;

  DISALLOW_COPY_AND_ASSIGN(IconDecodeRequest);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ICON_DECODE_REQUEST_H_
