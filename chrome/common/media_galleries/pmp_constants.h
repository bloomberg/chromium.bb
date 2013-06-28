// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_PMP_CONSTANTS_H_
#define CHROME_COMMON_MEDIA_GALLERIES_PMP_CONSTANTS_H_

#include <string>

#include "base/basictypes.h"
#include "base/time/time.h"

namespace picasa {

// PMP file format.
// Info derived from: http://sbktech.blogspot.com/2011/12/picasa-pmp-format.html

const char kPmpExtension[] = "pmp";

const base::Time::Exploded kPmpVariantTimeEpoch = {
  1899, 12, 7, 30,  // Dec 30, 1899 (Saturday)
  0, 0, 0, 0        // 00:00:00.000
};

const int64 kPmpHeaderSize = 20;

const int kPmpMagic1Offset = 0;
const int kPmpMagic2Offset = 6;
const int kPmpMagic3Offset = 8;
const int kPmpMagic4Offset = 14;

const uint32 kPmpMagic1 = 0x3fcccccd;
const uint16 kPmpMagic2 = 0x1332;
const uint32 kPmpMagic3 = 0x00000002;
const uint16 kPmpMagic4 = 0x1332;

const int kPmpFieldType1Offset = 4;
const int kPmpFieldType2Offset = 12;
const int kPmpRowCountOffset   = 16;

enum PmpFieldType {
  PMP_TYPE_STRING   = 0x00,
  PMP_TYPE_UINT32   = 0x01,
  PMP_TYPE_DOUBLE64 = 0x02,
  PMP_TYPE_UINT8    = 0x03,
  PMP_TYPE_UINT64   = 0x04,
  PMP_TYPE_INVALID  = 0xff
};

}  // namespace picasa

#endif  // CHROME_COMMON_MEDIA_GALLERIES_PMP_CONSTANTS_H_
