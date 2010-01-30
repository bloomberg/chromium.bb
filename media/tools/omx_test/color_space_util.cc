// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/tools/omx_test/color_space_util.h"

namespace media {

void NV21toIYUV(const uint8* nv21, uint8* iyuv, int width, int height) {
  memcpy(iyuv, nv21, width * height);
  iyuv += width * height;
  nv21 += width * height;
  uint8* u = iyuv;
  uint8* v = iyuv + width * height / 4;

  for (int i = 0; i < width * height / 4; ++i) {
    *v++ = *nv21++;
    *u++ = *nv21++;
  }
}

void NV21toYV12(const uint8* nv21, uint8* yv12, int width, int height) {
  memcpy(yv12, nv21, width * height);
  yv12 += width * height;
  nv21 += width * height;
  uint8* v = yv12;
  uint8* u = yv12 + width * height / 4;

  for (int i = 0; i < width * height / 4; ++i) {
    *v++ = *nv21++;
    *u++ = *nv21++;
  }
}

void IYUVtoNV21(const uint8* iyuv, uint8* nv21, int width, int height) {
  memcpy(nv21, iyuv, width * height);
  iyuv += width * height;
  nv21 += width * height;
  const uint8* u = iyuv;
  const uint8* v = iyuv + width * height / 4;

  for (int i = 0; i < width * height / 4; ++i) {
    *nv21++ = *v++;
    *nv21++ = *u++;
  }
}

void YV12toNV21(const uint8* yv12, uint8* nv21, int width, int height) {
  memcpy(nv21, yv12, width * height);
  yv12 += width * height;
  nv21 += width * height;
  const uint8* v = yv12;
  const uint8* u = yv12 + width * height / 4;

  for (int i = 0; i < width * height / 4; ++i) {
    *nv21++ = *v++;
    *nv21++ = *u++;
  }
}

}  // namespace media
