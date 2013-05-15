// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/traced_picture.h"

#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"

namespace cc {

TracedPicture::TracedPicture(scoped_refptr<Picture> picture)
  : picture_(picture) {
}

TracedPicture::~TracedPicture() {
}

scoped_ptr<base::debug::ConvertableToTraceFormat>
    TracedPicture::AsTraceablePicture(Picture* picture) {
  TracedPicture* ptr = new TracedPicture(picture);
  scoped_ptr<TracedPicture> result(ptr);
  return result.PassAs<base::debug::ConvertableToTraceFormat>();
}

void TracedPicture::AppendAsTraceFormat(std::string* out) const {
  std::string encoded_picture;
  picture_->AsBase64String(&encoded_picture);
  out->append("{");
  out->append("\"layer_rect\": [");
  base::StringAppendF(
      out, "%i,%i,%i,%i",
      picture_->LayerRect().x(), picture_->LayerRect().y(),
      picture_->LayerRect().width(), picture_->LayerRect().height());
  out->append("],");
  out->append("\"data_b64\": \"");
  out->append(encoded_picture);
  out->append("\"}");
}

}  // namespace cc
