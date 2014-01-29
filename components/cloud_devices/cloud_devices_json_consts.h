// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines strings constants for parsing Cloud Device Description.
// https://developers.google.com/cloud-print/docs/cdd

#ifndef COMPONENTS_CLOUD_DEVICES_CLOUD_DEVICES_JSON_CONSTANTS_H_
#define COMPONENTS_CLOUD_DEVICES_CLOUD_DEVICES_JSON_CONSTANTS_H_

#include <vector>

#include "base/memory/scoped_ptr.h"

namespace cloud_devices {

namespace cdd {

extern const char kVersion[];
extern const char kVersion10[];
extern const char kSectionPrinter[];

extern const char kKeyContentType[];
extern const char kKeyDefault[];
extern const char kKeyIsDefault[];
extern const char kKeyMax[];
extern const char kKeyType[];
extern const char kKeyOption[];
extern const char kKeyVendorId[];
extern const char kCustomName[];

extern const char kMargineBottomMicrons[];
extern const char kMargineLeftMicrons[];
extern const char kMargineRightMicrons[];
extern const char kMargineTopMicrons[];

extern const char kDpiHorizontal[];
extern const char kDpiVertical[];

extern const char kOptionCollate[];
extern const char kOptionColor[];
extern const char kOptionContentType[];
extern const char kOptionCopies[];
extern const char kOptionDpi[];
extern const char kOptionDuplex[];
extern const char kOptionFitToPage[];
extern const char kOptionMargins[];
extern const char kOptionMediaSize[];
extern const char kOptionPageOrientation[];
extern const char kOptionPageRange[];
extern const char kOptionReverse[];

extern const char kPageRangeEnd[];
extern const char kPageRangeStart[];

extern const char kTypeColorColor[];
extern const char kTypeColorMonochrome[];
extern const char kTypeColorCustomColor[];
extern const char kTypeColorCustomMonochrome[];
extern const char kTypeColorAuto[];

extern const char kTypeDuplexLongEdge[];
extern const char kTypeDuplexNoDuplex[];
extern const char kTypeDuplexShortEdge[];

extern const char kTypeFitToPageFillPage[];
extern const char kTypeFitToPageFitToPage[];
extern const char kTypeFitToPageGrowToPage[];
extern const char kTypeFitToPageNoFitting[];
extern const char kTypeFitToPageShrinkToPage[];

extern const char kTypeMarginsBorderless[];
extern const char kTypeMarginsCustom[];
extern const char kTypeMarginsStandard[];
extern const char kTypeOrientationAuto[];

extern const char kTypeOrientationLandscape[];
extern const char kTypeOrientationPortrait[];

}  // namespace  cdd

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_CLOUD_DEVICES_JSON_CONSTANTS_H_
