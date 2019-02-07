// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/printer_description.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_device_description_consts.h"
#include "components/cloud_devices/common/description_items_inl.h"

namespace cloud_devices {

namespace printer {

namespace {

const int32_t kMaxPageNumber = 1000000;

const char kSectionPrint[] = "print";
const char kSectionPrinter[] = "printer";

const char kKeyCustomDisplayName[] = "custom_display_name";
const char kKeyContentType[] = "content_type";
const char kKeyName[] = "name";
const char kKeyType[] = "type";
const char kKeyVendorId[] = "vendor_id";

// extern is required to be used in templates.
extern const char kOptionCollate[] = "collate";
extern const char kOptionColor[] = "color";
extern const char kOptionContentType[] = "supported_content_type";
extern const char kOptionCopies[] = "copies";
extern const char kOptionDpi[] = "dpi";
extern const char kOptionDuplex[] = "duplex";
extern const char kOptionFitToPage[] = "fit_to_page";
extern const char kOptionMargins[] = "margins";
extern const char kOptionMediaSize[] = "media_size";
extern const char kOptionPageOrientation[] = "page_orientation";
extern const char kOptionPageRange[] = "page_range";
extern const char kOptionReverse[] = "reverse_order";
extern const char kOptionPwgRasterConfig[] = "pwg_raster_config";

const char kMarginBottom[] = "bottom_microns";
const char kMarginLeft[] = "left_microns";
const char kMarginRight[] = "right_microns";
const char kMarginTop[] = "top_microns";

const char kDpiHorizontal[] = "horizontal_dpi";
const char kDpiVertical[] = "vertical_dpi";

const char kMediaWidth[] = "width_microns";
const char kMediaHeight[] = "height_microns";
const char kMediaIsContinuous[] = "is_continuous_feed";

const char kPageRangeInterval[] = "interval";
const char kPageRangeEnd[] = "end";
const char kPageRangeStart[] = "start";

const char kPwgRasterDocumentTypeSupported[] = "document_type_supported";
const char kPwgRasterDocumentSheetBack[] = "document_sheet_back";
const char kPwgRasterReverseOrderStreaming[] = "reverse_order_streaming";
const char kPwgRasterRotateAllPages[] = "rotate_all_pages";

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

const char kTypeDocumentSupportedTypeSRGB8[] = "SRGB_8";
const char kTypeDocumentSupportedTypeSGRAY8[] = "SGRAY_8";

const char kTypeDocumentSheetBackNormal[] = "NORMAL";
const char kTypeDocumentSheetBackRotated[] = "ROTATED";
const char kTypeDocumentSheetBackManualTumble[] = "MANUAL_TUMBLE";
const char kTypeDocumentSheetBackFlipped[] = "FLIPPED";

const struct ColorNames {
  ColorType id;
  const char* const json_name;
} kColorNames[] = {
      {STANDARD_COLOR, kTypeColorColor},
      {STANDARD_MONOCHROME, kTypeColorMonochrome},
      {CUSTOM_COLOR, kTypeColorCustomColor},
      {CUSTOM_MONOCHROME, kTypeColorCustomMonochrome},
      {AUTO_COLOR, kTypeColorAuto},
};

const struct DuplexNames {
  DuplexType id;
  const char* const json_name;
} kDuplexNames[] = {
      {NO_DUPLEX, kTypeDuplexNoDuplex},
      {LONG_EDGE, kTypeDuplexLongEdge},
      {SHORT_EDGE, kTypeDuplexShortEdge},
};

const struct OrientationNames {
  OrientationType id;
  const char* const json_name;
} kOrientationNames[] = {
      {PORTRAIT, kTypeOrientationPortrait},
      {LANDSCAPE, kTypeOrientationLandscape},
      {AUTO_ORIENTATION, kTypeOrientationAuto},
};

const struct MarginsNames {
  MarginsType id;
  const char* const json_name;
} kMarginsNames[] = {
      {NO_MARGINS, kTypeMarginsBorderless},
      {STANDARD_MARGINS, kTypeMarginsStandard},
      {CUSTOM_MARGINS, kTypeMarginsCustom},
};

const struct FitToPageNames {
  FitToPageType id;
  const char* const json_name;
} kFitToPageNames[] = {
      {NO_FITTING, kTypeFitToPageNoFitting},
      {FIT_TO_PAGE, kTypeFitToPageFitToPage},
      {GROW_TO_PAGE, kTypeFitToPageGrowToPage},
      {SHRINK_TO_PAGE, kTypeFitToPageShrinkToPage},
      {FILL_PAGE, kTypeFitToPageFillPage},
};

const struct DocumentSheetBackNames {
  DocumentSheetBack id;
  const char* const json_name;
} kDocumentSheetBackNames[] = {
      {NORMAL, kTypeDocumentSheetBackNormal},
      {ROTATED, kTypeDocumentSheetBackRotated},
      {MANUAL_TUMBLE, kTypeDocumentSheetBackManualTumble},
      {FLIPPED, kTypeDocumentSheetBackFlipped}};

const int32_t kInchToUm = 25400;
const int32_t kMmToUm = 1000;
const int32_t kSizeTrasholdUm = 1000;

#define MAP_CLOUD_PRINT_MEDIA_TYPE(type, width, height, unit_um) \
  {                                                              \
    type, #type, static_cast<int>(width* unit_um + 0.5),         \
        static_cast<int>(height* unit_um + 0.5)                  \
  }

const struct MediaDefinition {
  MediaType id;
  const char* const json_name;
  int width_um;
  int height_um;
} kMediaDefinitions[] = {
      {CUSTOM_MEDIA, "CUSTOM", 0, 0},
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_3X5, 3, 5, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_PERSONAL, 3.625f, 6.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_MONARCH, 3.875f, 7.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_9, 3.875f, 8.875f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_4X6, 4, 6, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_10, 4.125f, 9.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_A2, 4.375f, 5.75f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_11, 4.5f, 10.375f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_12, 4.75f, 11, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_5X7, 5, 7, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_5X8, 5, 8, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_14, 5, 11.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INVOICE, 5.5f, 8.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_4X6_EXT, 6, 8, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_6X9, 6, 9, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_C5, 6.5f, 9.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_7X9, 7, 9, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_EXECUTIVE, 7.25f, 10.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_GOVT_LETTER, 8, 10, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_GOVT_LEGAL, 8, 13, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_QUARTO, 8.5f, 10.83f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LETTER, 8.5f, 11, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_FANFOLD_EUR, 8.5f, 12, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LETTER_PLUS, 8.5f, 12.69f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_FOOLSCAP, 8.5f, 13, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LEGAL, 8.5f, 14, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_SUPER_A, 8.94f, 14, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_9X11, 9, 11, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_A, 9, 12, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LETTER_EXTRA, 9.5f, 12, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LEGAL_EXTRA, 9.5f, 15, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X11, 10, 11, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X13, 10, 13, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X14, 10, 14, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X15, 10, 15, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_11X12, 11, 12, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_EDP, 11, 14, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_FANFOLD_US, 11, 14.875f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_11X15, 11, 15, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LEDGER, 11, 17, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_EUR_EDP, 12, 14, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_B, 12, 18, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_12X19, 12, 19, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_B_PLUS, 12, 19.17f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_SUPER_B, 13, 19, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_C, 17, 22, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_C, 18, 24, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_D, 22, 34, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_D, 24, 36, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ASME_F, 28, 40, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_WIDE_FORMAT, 30, 42, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_E, 34, 44, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_E, 36, 48, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(NA_F, 44, 68, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ROC_16K, 7.75f, 10.75f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ROC_8K, 10.75f, 15.5f, kInchToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_32K, 97, 151, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_1, 102, 165, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_2, 102, 176, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_4, 110, 208, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_5, 110, 220, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_8, 120, 309, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_6, 120, 230, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_3, 125, 176, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_16K, 146, 215, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_7, 160, 230, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_JUURO_KU_KAI, 198, 275, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_PA_KAI, 267, 389, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_DAI_PA_KAI, 275, 395, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_10, 324, 458, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A10, 26, 37, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A9, 37, 52, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A8, 52, 74, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A7, 74, 105, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A6, 105, 148, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A5, 148, 210, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A5_EXTRA, 174, 235, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4, 210, 297, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4_TAB, 225, 297, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4_EXTRA, 235, 322, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3, 297, 420, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X3, 297, 630, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X4, 297, 841, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X5, 297, 1051, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X6, 297, 1261, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X7, 297, 1471, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X8, 297, 1682, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X9, 297, 1892, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3_EXTRA, 322, 445, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2, 420, 594, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X3, 420, 891, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X4, 420, 1189, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X5, 420, 1486, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X6, 420, 1783, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X7, 420, 2080, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A1, 594, 841, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2X3, 594, 1261, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2X4, 594, 1682, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2X5, 594, 2102, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A0, 841, 1189, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A1X3, 841, 1783, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A1X4, 841, 2378, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_2A0, 1189, 1682, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A0X3, 1189, 2523, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B10, 31, 44, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B9, 44, 62, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B8, 62, 88, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B7, 88, 125, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B6, 125, 176, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B6C4, 125, 324, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B5, 176, 250, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B5_EXTRA, 201, 276, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B4, 250, 353, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B3, 353, 500, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B2, 500, 707, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B1, 707, 1000, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B0, 1000, 1414, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C10, 28, 40, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C9, 40, 57, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C8, 57, 81, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C7, 81, 114, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C7C6, 81, 162, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C6, 114, 162, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C6C5, 114, 229, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C5, 162, 229, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C4, 229, 324, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C3, 324, 458, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C2, 458, 648, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C1, 648, 917, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C0, 917, 1297, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_DL, 110, 220, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_RA2, 430, 610, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_SRA2, 450, 640, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_RA1, 610, 860, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_SRA1, 640, 900, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_RA0, 860, 1220, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_SRA0, 900, 1280, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B10, 32, 45, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B9, 45, 64, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B8, 64, 91, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B7, 91, 128, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B6, 128, 182, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B5, 182, 257, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B4, 257, 364, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B3, 364, 515, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B2, 515, 728, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B1, 728, 1030, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B0, 1030, 1456, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_EXEC, 216, 330, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_CHOU4, 90, 205, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_HAGAKI, 100, 148, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_YOU4, 105, 235, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_CHOU2, 111.1f, 146, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_CHOU3, 120, 235, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_OUFUKU, 148, 200, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_KAHU, 240, 322.1f, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_KAKU2, 240, 332, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_SMALL_PHOTO, 100, 150, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_ITALIAN, 110, 230, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_POSTFIX, 114, 229, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_LARGE_PHOTO, 200, 300, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_FOLIO, 210, 330, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_FOLIO_SP, 215, 315, kMmToUm),
      MAP_CLOUD_PRINT_MEDIA_TYPE(OM_INVITE, 220, 220, kMmToUm)};
#undef MAP_CLOUD_PRINT_MEDIA_TYPE

const MediaDefinition& FindMediaByType(MediaType type) {
  for (size_t i = 0; i < base::size(kMediaDefinitions); ++i) {
    if (kMediaDefinitions[i].id == type)
      return kMediaDefinitions[i];
  }
  NOTREACHED();
  return kMediaDefinitions[0];
}

const MediaDefinition* FindMediaBySize(int32_t width_um, int32_t height_um) {
  const MediaDefinition* result = nullptr;
  for (size_t i = 0; i < base::size(kMediaDefinitions); ++i) {
    int32_t diff =
        std::max(std::abs(width_um - kMediaDefinitions[i].width_um),
                 std::abs(height_um - kMediaDefinitions[i].height_um));
    if (diff < kSizeTrasholdUm)
      result = &kMediaDefinitions[i];
  }
  return result;
}

template <class T, class IdType>
std::string TypeToString(const T& names, IdType id) {
  for (size_t i = 0; i < base::size(names); ++i) {
    if (id == names[i].id)
      return names[i].json_name;
  }
  NOTREACHED();
  return std::string();
}

template <class T, class IdType>
bool TypeFromString(const T& names, const std::string& type, IdType* id) {
  for (size_t i = 0; i < base::size(names); ++i) {
    if (type == names[i].json_name) {
      *id = names[i].id;
      return true;
    }
  }
  return false;
}

}  // namespace

PwgRasterConfig::PwgRasterConfig()
    : document_sheet_back(ROTATED),
      reverse_order_streaming(false),
      rotate_all_pages(false) {
}

PwgRasterConfig::~PwgRasterConfig() {}

Color::Color() : type(AUTO_COLOR) {
}

Color::Color(ColorType type) : type(type) {
}

bool Color::operator==(const Color& other) const {
  return type == other.type && vendor_id == other.vendor_id &&
         custom_display_name == other.custom_display_name;
}

bool Color::IsValid() const {
  if (type != CUSTOM_COLOR && type != CUSTOM_MONOCHROME)
    return true;
  return !vendor_id.empty() && !custom_display_name.empty();
}

Margins::Margins()
    : type(STANDARD_MARGINS), top_um(0), right_um(0), bottom_um(0), left_um(0) {
}

Margins::Margins(MarginsType type,
                 int32_t top_um,
                 int32_t right_um,
                 int32_t bottom_um,
                 int32_t left_um)
    : type(type),
      top_um(top_um),
      right_um(right_um),
      bottom_um(bottom_um),
      left_um(left_um) {}

bool Margins::operator==(const Margins& other) const {
  return type == other.type && top_um == other.top_um &&
         right_um == other.right_um && bottom_um == other.bottom_um;
}

Dpi::Dpi() : horizontal(0), vertical(0) {
}

Dpi::Dpi(int32_t horizontal, int32_t vertical)
    : horizontal(horizontal), vertical(vertical) {}

bool Dpi::IsValid() const {
  return horizontal > 0 && vertical > 0;
}

bool Dpi::operator==(const Dpi& other) const {
  return horizontal == other.horizontal && vertical == other.vertical;
}

Media::Media()
    : type(CUSTOM_MEDIA), width_um(0), height_um(0), is_continuous_feed(false) {
}

Media::Media(MediaType type)
    : type(type),
      width_um(0),
      height_um(0),
      is_continuous_feed(false) {
  const MediaDefinition& media = FindMediaByType(type);
  width_um = media.width_um;
  height_um = media.height_um;
  is_continuous_feed = width_um <= 0 || height_um <= 0;
}

Media::Media(MediaType type, int32_t width_um, int32_t height_um)
    : type(type),
      width_um(width_um),
      height_um(height_um),
      is_continuous_feed(width_um <= 0 || height_um <= 0) {}

Media::Media(const std::string& custom_display_name,
             const std::string& vendor_id,
             int32_t width_um,
             int32_t height_um)
    : type(CUSTOM_MEDIA),
      width_um(width_um),
      height_um(height_um),
      is_continuous_feed(width_um <= 0 || height_um <= 0),
      custom_display_name(custom_display_name),
      vendor_id(vendor_id) {}

Media::Media(const Media& other) = default;

bool Media::MatchBySize() {
  const MediaDefinition* media = FindMediaBySize(width_um, height_um);
  if (!media)
    return false;
  type = media->id;
  return true;
}

bool Media::IsValid() const {
  if (is_continuous_feed) {
    if (width_um <= 0 && height_um <= 0)
      return false;
  } else {
    if (width_um <= 0 || height_um <= 0)
      return false;
  }
  return true;
}

bool Media::operator==(const Media& other) const {
  return type == other.type && width_um == other.width_um &&
         height_um == other.height_um &&
         is_continuous_feed == other.is_continuous_feed;
}

Interval::Interval() : start(0), end(0) {
}

Interval::Interval(int32_t start, int32_t end) : start(start), end(end) {}

Interval::Interval(int32_t start) : start(start), end(kMaxPageNumber) {}

bool Interval::operator==(const Interval& other) const {
  return start == other.start && end == other.end;
}

template <const char* kName>
class ItemsTraits {
 public:
  static std::vector<base::StringPiece> GetCapabilityPath() {
    return {kSectionPrinter, kName};
  }

  static std::vector<base::StringPiece> GetTicketItemPath() {
    return {kSectionPrint, kName};
  }
};

class NoValueValidation {
 public:
  template <class Option>
  static bool IsValid(const Option&) {
    return true;
  }
};

class ContentTypeTraits : public NoValueValidation,
                          public ItemsTraits<kOptionContentType> {
 public:
  static bool Load(const base::Value& dict, ContentType* option) {
    const std::string* content_type = dict.FindStringKey(kKeyContentType);
    if (!content_type)
      return false;
    *option = *content_type;
    return true;
  }

  static void Save(ContentType option, base::Value* dict) {
    dict->SetKey(kKeyContentType, base::Value(option));
  }
};

class PwgRasterConfigTraits : public NoValueValidation,
                              public ItemsTraits<kOptionPwgRasterConfig> {
 public:
  static bool Load(const base::Value& dict, PwgRasterConfig* option) {
    PwgRasterConfig option_out;
    const base::Value* document_sheet_back =
        dict.FindKey(kPwgRasterDocumentSheetBack);
    if (document_sheet_back) {
      if (!document_sheet_back->is_string() ||
          !TypeFromString(kDocumentSheetBackNames,
                          document_sheet_back->GetString(),
                          &option_out.document_sheet_back)) {
        return false;
      }
    }

    const base::Value* document_types_supported =
        dict.FindKey(kPwgRasterDocumentTypeSupported);
    if (document_types_supported) {
      if (!document_types_supported->is_list())
        return false;
      for (const auto& type_value : document_types_supported->GetList()) {
        if (!type_value.is_string())
          return false;

        const std::string& type = type_value.GetString();
        if (type == kTypeDocumentSupportedTypeSRGB8)
          option_out.document_types_supported.push_back(SRGB_8);
        else if (type == kTypeDocumentSupportedTypeSGRAY8)
          option_out.document_types_supported.push_back(SGRAY_8);
      }
    }

    option_out.reverse_order_streaming =
        dict.FindBoolKey(kPwgRasterReverseOrderStreaming).value_or(false);
    option_out.rotate_all_pages =
        dict.FindBoolKey(kPwgRasterRotateAllPages).value_or(false);
    *option = option_out;
    return true;
  }

  static void Save(const PwgRasterConfig& option, base::Value* dict) {
    dict->SetKey(kPwgRasterDocumentSheetBack,
                 base::Value(TypeToString(kDocumentSheetBackNames,
                                          option.document_sheet_back)));

    if (!option.document_types_supported.empty()) {
      base::Value::ListStorage supported_list;
      for (const auto& type : option.document_types_supported) {
        switch (type) {
          case SRGB_8:
            supported_list.push_back(
                base::Value(kTypeDocumentSupportedTypeSRGB8));
            break;
          case SGRAY_8:
            supported_list.push_back(
                base::Value(kTypeDocumentSupportedTypeSGRAY8));
            break;
        }
      }
      dict->SetKey(kPwgRasterDocumentTypeSupported,
                   base::Value(supported_list));
    }

    if (option.reverse_order_streaming) {
      dict->SetKey(kPwgRasterReverseOrderStreaming,
                   base::Value(option.reverse_order_streaming));
    }

    if (option.rotate_all_pages) {
      dict->SetKey(kPwgRasterRotateAllPages,
                   base::Value(option.rotate_all_pages));
    }
  }
};

class ColorTraits : public ItemsTraits<kOptionColor> {
 public:
  static bool IsValid(const Color& option) { return option.IsValid(); }

  static bool Load(const base::Value& dict, Color* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    if (!type || !TypeFromString(kColorNames, *type, &option->type))
      return false;
    const std::string* vendor_id = dict.FindStringKey(kKeyVendorId);
    if (vendor_id)
      option->vendor_id = *vendor_id;
    const std::string* custom_display_name =
        dict.FindStringKey(kKeyCustomDisplayName);
    if (custom_display_name)
      option->custom_display_name = *custom_display_name;
    return true;
  }

  static void Save(const Color& option, base::Value* dict) {
    dict->SetKey(kKeyType, base::Value(TypeToString(kColorNames, option.type)));
    if (!option.vendor_id.empty())
      dict->SetKey(kKeyVendorId, base::Value(option.vendor_id));
    if (!option.custom_display_name.empty()) {
      dict->SetKey(kKeyCustomDisplayName,
                   base::Value(option.custom_display_name));
    }
  }
};

class DuplexTraits : public NoValueValidation,
                     public ItemsTraits<kOptionDuplex> {
 public:
  static bool Load(const base::Value& dict, DuplexType* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    return type && TypeFromString(kDuplexNames, *type, option);
  }

  static void Save(DuplexType option, base::Value* dict) {
    dict->SetKey(kKeyType, base::Value(TypeToString(kDuplexNames, option)));
  }
};

class OrientationTraits : public NoValueValidation,
                          public ItemsTraits<kOptionPageOrientation> {
 public:
  static bool Load(const base::Value& dict, OrientationType* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    return type && TypeFromString(kOrientationNames, *type, option);
  }

  static void Save(OrientationType option, base::Value* dict) {
    dict->SetKey(kKeyType,
                 base::Value(TypeToString(kOrientationNames, option)));
  }
};

class CopiesTraits : public ItemsTraits<kOptionCopies> {
 public:
  static bool IsValid(int32_t option) { return option >= 1; }

  static bool Load(const base::Value& dict, int32_t* option) {
    base::Optional<int> copies = dict.FindIntKey(kOptionCopies);
    if (!copies)
      return false;
    *option = copies.value();
    return true;
  }

  static void Save(int32_t option, base::Value* dict) {
    dict->SetKey(kOptionCopies, base::Value(option));
  }
};

class MarginsTraits : public NoValueValidation,
                      public ItemsTraits<kOptionMargins> {
 public:
  static bool Load(const base::Value& dict, Margins* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    if (!type || !TypeFromString(kMarginsNames, *type, &option->type))
      return false;
    base::Optional<int> top_um = dict.FindIntKey(kMarginTop);
    base::Optional<int> right_um = dict.FindIntKey(kMarginRight);
    base::Optional<int> bottom_um = dict.FindIntKey(kMarginBottom);
    base::Optional<int> left_um = dict.FindIntKey(kMarginLeft);
    if (!top_um || !right_um || !bottom_um || !left_um)
      return false;
    option->top_um = top_um.value();
    option->right_um = right_um.value();
    option->bottom_um = bottom_um.value();
    option->left_um = left_um.value();
    return true;
  }

  static void Save(const Margins& option, base::Value* dict) {
    dict->SetKey(kKeyType,
                 base::Value(TypeToString(kMarginsNames, option.type)));
    dict->SetKey(kMarginTop, base::Value(option.top_um));
    dict->SetKey(kMarginRight, base::Value(option.right_um));
    dict->SetKey(kMarginBottom, base::Value(option.bottom_um));
    dict->SetKey(kMarginLeft, base::Value(option.left_um));
  }
};

class DpiTraits : public ItemsTraits<kOptionDpi> {
 public:
  static bool IsValid(const Dpi& option) { return option.IsValid(); }

  static bool Load(const base::Value& dict, Dpi* option) {
    base::Optional<int> horizontal = dict.FindIntKey(kDpiHorizontal);
    base::Optional<int> vertical = dict.FindIntKey(kDpiVertical);
    if (!horizontal || !vertical)
      return false;
    option->horizontal = horizontal.value();
    option->vertical = vertical.value();
    return true;
  }

  static void Save(const Dpi& option, base::Value* dict) {
    dict->SetKey(kDpiHorizontal, base::Value(option.horizontal));
    dict->SetKey(kDpiVertical, base::Value(option.vertical));
  }
};

class FitToPageTraits : public NoValueValidation,
                        public ItemsTraits<kOptionFitToPage> {
 public:
  static bool Load(const base::Value& dict, FitToPageType* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    return type && TypeFromString(kFitToPageNames, *type, option);
  }

  static void Save(FitToPageType option, base::Value* dict) {
    dict->SetKey(kKeyType, base::Value(TypeToString(kFitToPageNames, option)));
  }
};

class PageRangeTraits : public ItemsTraits<kOptionPageRange> {
 public:
  static bool IsValid(const PageRange& option) {
    for (size_t i = 0; i < option.size(); ++i) {
      if (option[i].start < 1 || option[i].end < 1) {
        return false;
      }
    }
    return true;
  }

  static bool Load(const base::Value& dict, PageRange* option) {
    const base::Value* list_value =
        dict.FindKeyOfType(kPageRangeInterval, base::Value::Type::LIST);
    if (!list_value)
      return false;
    const base::Value::ListStorage& list = list_value->GetList();
    for (const base::Value& interval : list) {
      int page_range_start = interval.FindIntKey(kPageRangeStart).value_or(1);
      int page_range_end =
          interval.FindIntKey(kPageRangeEnd).value_or(kMaxPageNumber);
      option->push_back(Interval(page_range_start, page_range_end));
    }
    return true;
  }

  static void Save(const PageRange& option, base::Value* dict) {
    if (!option.empty()) {
      base::Value list(base::Value::Type::LIST);
      for (size_t i = 0; i < option.size(); ++i) {
        base::Value interval(base::Value::Type::DICTIONARY);
        interval.SetKey(kPageRangeStart, base::Value(option[i].start));
        if (option[i].end < kMaxPageNumber)
          interval.SetKey(kPageRangeEnd, base::Value(option[i].end));
        list.GetList().emplace_back(std::move(interval));
      }
      dict->SetKey(kPageRangeInterval, std::move(list));
    }
  }
};

class MediaTraits : public ItemsTraits<kOptionMediaSize> {
 public:
  static bool IsValid(const Media& option) { return option.IsValid(); }

  static bool Load(const base::Value& dict, Media* option) {
    const std::string* type = dict.FindStringKey(kKeyName);
    if (type && !TypeFromString(kMediaDefinitions, *type, &option->type))
      return false;
    base::Optional<int> width_um = dict.FindIntKey(kMediaWidth);
    if (width_um)
      option->width_um = width_um.value();
    base::Optional<int> height_um = dict.FindIntKey(kMediaHeight);
    if (height_um)
      option->height_um = height_um.value();
    base::Optional<bool> is_continuous_feed =
        dict.FindBoolKey(kMediaIsContinuous);
    if (is_continuous_feed)
      option->is_continuous_feed = is_continuous_feed.value();
    const std::string* custom_display_name =
        dict.FindStringKey(kKeyCustomDisplayName);
    if (custom_display_name)
      option->custom_display_name = *custom_display_name;
    const std::string* vendor_id = dict.FindStringKey(kKeyVendorId);
    if (vendor_id)
      option->vendor_id = *vendor_id;
    return true;
  }

  static void Save(const Media& option, base::Value* dict) {
    if (option.type != CUSTOM_MEDIA) {
      dict->SetKey(kKeyName,
                   base::Value(TypeToString(kMediaDefinitions, option.type)));
    }
    if (!option.custom_display_name.empty() || option.type == CUSTOM_MEDIA) {
      dict->SetKey(kKeyCustomDisplayName,
                   base::Value(option.custom_display_name));
    }
    if (!option.vendor_id.empty())
      dict->SetKey(kKeyVendorId, base::Value(option.vendor_id));
    if (option.width_um > 0)
      dict->SetKey(kMediaWidth, base::Value(option.width_um));
    if (option.height_um > 0)
      dict->SetKey(kMediaHeight, base::Value(option.height_um));
    if (option.is_continuous_feed)
      dict->SetKey(kMediaIsContinuous, base::Value(true));
  }
};

class CollateTraits : public NoValueValidation,
                      public ItemsTraits<kOptionCollate> {
 public:
  static const bool kDefault = true;

  static bool Load(const base::Value& dict, bool* option) {
    base::Optional<bool> collate = dict.FindBoolKey(kOptionCollate);
    if (!collate)
      return false;
    *option = collate.value();
    return true;
  }

  static void Save(bool option, base::Value* dict) {
    dict->SetKey(kOptionCollate, base::Value(option));
  }
};

class ReverseTraits : public NoValueValidation,
                      public ItemsTraits<kOptionReverse> {
 public:
  static const bool kDefault = false;

  static bool Load(const base::Value& dict, bool* option) {
    base::Optional<bool> reverse = dict.FindBoolKey(kOptionReverse);
    if (!reverse)
      return false;
    *option = reverse.value();
    return true;
  }

  static void Save(bool option, base::Value* dict) {
    dict->SetKey(kOptionReverse, base::Value(option));
  }
};

}  // namespace printer

using namespace printer;

template class ListCapability<ContentType, ContentTypeTraits>;
template class ValueCapability<PwgRasterConfig, PwgRasterConfigTraits>;
template class SelectionCapability<Color, ColorTraits>;
template class SelectionCapability<DuplexType, DuplexTraits>;
template class SelectionCapability<OrientationType, OrientationTraits>;
template class SelectionCapability<Margins, MarginsTraits>;
template class SelectionCapability<Dpi, DpiTraits>;
template class SelectionCapability<FitToPageType, FitToPageTraits>;
template class SelectionCapability<Media, MediaTraits>;
template class EmptyCapability<class CopiesTraits>;
template class EmptyCapability<class PageRangeTraits>;
template class BooleanCapability<class CollateTraits>;
template class BooleanCapability<class ReverseTraits>;

template class TicketItem<PwgRasterConfig, PwgRasterConfigTraits>;
template class TicketItem<Color, ColorTraits>;
template class TicketItem<DuplexType, DuplexTraits>;
template class TicketItem<OrientationType, OrientationTraits>;
template class TicketItem<Margins, MarginsTraits>;
template class TicketItem<Dpi, DpiTraits>;
template class TicketItem<FitToPageType, FitToPageTraits>;
template class TicketItem<Media, MediaTraits>;
template class TicketItem<int32_t, CopiesTraits>;
template class TicketItem<PageRange, PageRangeTraits>;
template class TicketItem<bool, CollateTraits>;
template class TicketItem<bool, ReverseTraits>;

}  // namespace cloud_devices
