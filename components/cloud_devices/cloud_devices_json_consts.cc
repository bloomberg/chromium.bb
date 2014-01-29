// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/cloud_devices_json_consts.h"

namespace cloud_devices {

namespace cdd {

const char kVersion[] = "version";
const char kVersion10[] = "1.0";
const char kSectionPrinter[] = "printer";

const char kKeyContentType[] = "content_type";
const char kKeyDefault[] = "default";
const char kKeyIsDefault[] = "is_default";
const char kKeyMax[] = "max";
const char kKeyType[] = "type";
const char kKeyOption[] = "option";
const char kKeyVendorId[] = "vendor_id";
const char kCustomName[] = "custom_display_name";

const char kMargineBottomMicrons[] = "bottom_microns";
const char kMargineLeftMicrons[] = "left_microns";
const char kMargineRightMicrons[] = "right_microns";
const char kMargineTopMicrons[] = "top_microns";

const char kDpiHorizontal[] = "horizontal_dpi";
const char kDpiVertical[] = "vertical_dpi";

const char kOptionCollate[] = "collate";
const char kOptionColor[] = "color";
const char kOptionContentType[] = "supported_content_type";
const char kOptionCopies[] = "copies";
const char kOptionDpi[] = "dpi";
const char kOptionDuplex[] = "duplex";
const char kOptionFitToPage[] = "fit_to_page";
const char kOptionMargins[] = "margins";
const char kOptionMediaSize[] = "media_size";
const char kOptionPageOrientation[] = "page_orientation";
const char kOptionPageRange[] = "page_range";
const char kOptionReverse[] = "reverse_order";

const char kPageRangeEnd[] = "end";
const char kPageRangeStart[] = "start";

const char kTypeColorColor[] = "STANDARD_COLOR";
const char kTypeColorMonochrome[] = "STANDARD_MONOCHROME";
const char kTypeColorCustomColor[] = "CUSTOM_COLOR";
const char kTypeColorCustomMonochrome[] = "CUSTOM_MONOCHROME";
const char kTypeColorAuto[] = "AUTO";

const char kTypeDuplexLongEdge[] = "LONG_EDGE";
const char kTypeDuplexNoDuplex[] = "NO_DUPLEX";
const char kTypeDuplexShortEdge[] = "SHORT_EDGE";

const char kTypeFitToPageFillPage[] = "FILL_PAGE";
const char kTypeFitToPageFitToPage[] = "FIT_TO_PAGE";
const char kTypeFitToPageGrowToPage[] = "GROW_TO_PAGE";
const char kTypeFitToPageNoFitting[] = "NO_FITTING";
const char kTypeFitToPageShrinkToPage[] = "SHRINK_TO_PAGE";

const char kTypeMarginsBorderless[] = "BORDERLESS";
const char kTypeMarginsCustom[] = "CUSTOM";
const char kTypeMarginsStandard[] = "STANDARD";
const char kTypeOrientationAuto[] = "AUTO";

const char kTypeOrientationLandscape[] = "LANDSCAPE";
const char kTypeOrientationPortrait[] = "PORTRAIT";

}  // namespace  cdd

}  // namespace cloud_devices
