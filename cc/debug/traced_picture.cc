// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/traced_picture.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
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
  scoped_ptr<base::Value> value = picture_->AsValue();
  std::string tmp;
  base::JSONWriter::Write(value.get(), &tmp);
  out->append(tmp);
}

}  // namespace cc
