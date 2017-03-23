// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_color_space.h"

namespace media {

VideoColorSpace::PrimaryID VideoColorSpace::GetPrimaryID(int primary) {
  if (primary < 1 || primary > 22 || primary == 3)
    return PrimaryID::INVALID;
  if (primary > 12 && primary < 22)
    return PrimaryID::INVALID;
  return static_cast<PrimaryID>(primary);
}

VideoColorSpace::TransferID VideoColorSpace::GetTransferID(int transfer) {
  if (transfer < 1 || transfer > 18 || transfer == 3)
    return TransferID::INVALID;
  return static_cast<TransferID>(transfer);
}

VideoColorSpace::MatrixID VideoColorSpace::GetMatrixID(int matrix) {
  if (matrix < 0 || matrix > 11 || matrix == 3)
    return MatrixID::INVALID;
  return static_cast<MatrixID>(matrix);
}

VideoColorSpace::VideoColorSpace() {}

VideoColorSpace::VideoColorSpace(PrimaryID primaries,
                                 TransferID transfer,
                                 MatrixID matrix,
                                 gfx::ColorSpace::RangeID range)
    : primaries(primaries), transfer(transfer), matrix(matrix), range(range) {}

VideoColorSpace::VideoColorSpace(int primaries,
                                 int transfer,
                                 int matrix,
                                 gfx::ColorSpace::RangeID range)
    : primaries(GetPrimaryID(primaries)),
      transfer(GetTransferID(transfer)),
      matrix(GetMatrixID(matrix)),
      range(range) {}

bool VideoColorSpace::operator==(const VideoColorSpace& other) const {
  return primaries == other.primaries && transfer == other.transfer &&
         matrix == other.matrix && range == other.range;
}

bool VideoColorSpace::operator!=(const VideoColorSpace& other) const {
  return primaries != other.primaries || transfer != other.transfer ||
         matrix != other.matrix || range != other.range;
}

gfx::ColorSpace VideoColorSpace::ToGfxColorSpace() const {
  // TODO(hubbe): Make this type-safe.
  return gfx::ColorSpace::CreateVideo(static_cast<int>(primaries),
                                      static_cast<int>(transfer),
                                      static_cast<int>(matrix), range);
}

VideoColorSpace VideoColorSpace::REC709() {
  return VideoColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                         gfx::ColorSpace::RangeID::LIMITED);
}
VideoColorSpace VideoColorSpace::REC601() {
  return VideoColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                         MatrixID::SMPTE170M,
                         gfx::ColorSpace::RangeID::LIMITED);
}
VideoColorSpace VideoColorSpace::JPEG() {
  // TODO(ccameron): Determine which primaries and transfer function were
  // intended here.
  return VideoColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1,
                         MatrixID::SMPTE170M, gfx::ColorSpace::RangeID::FULL);
}

}  // namespace
