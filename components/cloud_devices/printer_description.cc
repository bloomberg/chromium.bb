// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/printer_description.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/cloud_devices/cloud_device_description_consts.h"
#include "components/cloud_devices/description_items_inl.h"

namespace cloud_devices {

namespace printer {

namespace {

const int32 kMaxPageNumber = 1000000;

const char kSectionPrinter[] = "printer";

const char kCustomName[] = "custom_display_name";
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

const char kMargineBottomMicrons[] = "bottom_microns";
const char kMargineLeftMicrons[] = "left_microns";
const char kMargineRightMicrons[] = "right_microns";
const char kMargineTopMicrons[] = "top_microns";

const char kDpiHorizontal[] = "horizontal_dpi";
const char kDpiVertical[] = "vertical_dpi";

const char kMediaWidth[] = "width_microns";
const char kMediaHeight[] = "height_microns";
const char kMediaIsContinuous[] = "is_continuous_feed";

const char kPageRangeInterval[] = "interval";
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

template <class IdType>
struct TypePair {
  IdType id;
  const char* const json_name;
  static const TypePair kTypeMap[];
};

template<>
const TypePair<ColorType> TypePair<ColorType>::kTypeMap[] = {
  { STANDARD_COLOR, kTypeColorColor },
  { STANDARD_MONOCHROME, kTypeColorMonochrome },
  { CUSTOM_COLOR, kTypeColorCustomColor },
  { CUSTOM_MONOCHROME, kTypeColorCustomMonochrome },
  { AUTO_COLOR, kTypeColorAuto },
};

template<>
const TypePair<DuplexType>
    TypePair<DuplexType>::kTypeMap[] = {
  { NO_DUPLEX, kTypeDuplexNoDuplex },
  { LONG_EDGE, kTypeDuplexLongEdge },
  { SHORT_EDGE, kTypeDuplexShortEdge },
};

template<>
const TypePair<OrientationType>
    TypePair<OrientationType>::kTypeMap[] = {
  { PORTRAIT, kTypeOrientationPortrait },
  { LANDSCAPE, kTypeOrientationLandscape },
  { AUTO_ORIENTATION, kTypeOrientationAuto },
};

template<>
const TypePair<MarginsType>
    TypePair<MarginsType>::kTypeMap[] = {
  { NO_MARGINS, kTypeMarginsBorderless },
  { STANDARD_MARGINS, kTypeMarginsStandard },
  { CUSTOM_MARGINS, kTypeMarginsCustom },
};

template<>
const TypePair<FitToPageType>
    TypePair<FitToPageType>::kTypeMap[] = {
  { NO_FITTING, kTypeFitToPageNoFitting },
  { FIT_TO_PAGE, kTypeFitToPageFitToPage },
  { GROW_TO_PAGE, kTypeFitToPageGrowToPage },
  { SHRINK_TO_PAGE, kTypeFitToPageShrinkToPage },
  { FILL_PAGE, kTypeFitToPageFillPage },
};


template<>
const TypePair<MediaType>
    TypePair<MediaType>::kTypeMap[] = {
#define MAP_CLOUD_PRINT_MEDIA_TYPE(type) { type, #type }
  { CUSTOM_MEDIA, "CUSTOM" },
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_3X5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_PERSONAL),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_MONARCH),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_4X6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_10),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_A2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_11),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_12),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_5X7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_5X8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_NUMBER_14),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INVOICE),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_INDEX_4X6_EXT),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_6X9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_C5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_7X9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_EXECUTIVE),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_GOVT_LETTER),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_GOVT_LEGAL),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_QUARTO),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LETTER),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_FANFOLD_EUR),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LETTER_PLUS),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_FOOLSCAP),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LEGAL),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_SUPER_A),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_9X11),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_A),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LETTER_EXTRA),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LEGAL_EXTRA),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X11),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X13),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X14),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_10X15),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_11X12),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_EDP),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_FANFOLD_US),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_11X15),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_LEDGER),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_EUR_EDP),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_B),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_12X19),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_B_PLUS),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_SUPER_B),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_C),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_C),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_D),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_D),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ASME_F),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_WIDE_FORMAT),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_E),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_ARCH_E),
  MAP_CLOUD_PRINT_MEDIA_TYPE(NA_F),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ROC_16K),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ROC_8K),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_32K),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_16K),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_JUURO_KU_KAI),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_PA_KAI),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_DAI_PA_KAI),
  MAP_CLOUD_PRINT_MEDIA_TYPE(PRC_10),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A10),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A5_EXTRA),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4_TAB),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4_EXTRA),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A4X9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3_EXTRA),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A3X7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2X3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2X4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A2X5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A1X3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A1X4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_2A0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_A0X3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B10),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B6C4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B5_EXTRA),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_B0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C10),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C7C6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C6C5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_C0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_DL),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_RA2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_SRA2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_RA1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_SRA1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_RA0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(ISO_SRA0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B10),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B9),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B8),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B7),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B6),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B5),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B1),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_B0),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JIS_EXEC),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_CHOU4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_HAGAKI),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_YOU4),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_CHOU2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_CHOU3),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_OUFUKU),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_KAHU),
  MAP_CLOUD_PRINT_MEDIA_TYPE(JPN_KAKU2),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_SMALL_PHOTO),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_ITALIAN),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_POSTFIX),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_LARGE_PHOTO),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_FOLIO),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_FOLIO_SP),
  MAP_CLOUD_PRINT_MEDIA_TYPE(OM_INVITE),
#undef MAP_CLOUD_PRINT_MEDIA_TYPE
};

template<class IdType>
std::string TypeToString(IdType id) {
  for (size_t i = 0; i < arraysize(TypePair<IdType>::kTypeMap); ++i) {
    if (id == TypePair<IdType>::kTypeMap[i].id)
      return TypePair<IdType>::kTypeMap[i].json_name;
  }
  NOTREACHED();
  return std::string();
}

template<class IdType>
bool TypeFromString(const std::string& type, IdType* id) {
  for (size_t i = 0; i < arraysize(TypePair<IdType>::kTypeMap); ++i) {
    if (type == TypePair<IdType>::kTypeMap[i].json_name) {
      *id = TypePair<IdType>::kTypeMap[i].id;
      return true;
    }
  }
  return false;
}

}  // namespace

Color::Color() : type(AUTO_COLOR) {}

Color::Color(ColorType type) : type(type) {}

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
    : type(STANDARD_MARGINS),
      top_microns(0),
      right_microns(0),
      bottom_microns(0),
      left_microns(0) {
}

Margins::Margins(MarginsType type,
                 int32 top_microns,
                 int32 right_microns,
                 int32 bottom_microns,
                 int32 left_microns)
    : type(type),
      top_microns(top_microns),
      right_microns(right_microns),
      bottom_microns(bottom_microns),
      left_microns(left_microns) {
}

bool Margins::operator==(const Margins& other) const {
  return type == other.type &&
         top_microns == other.top_microns &&
         right_microns == other.right_microns &&
         bottom_microns == other.bottom_microns;
}

Dpi::Dpi() : horizontal(0), vertical(0) {}

Dpi::Dpi(int32 horizontal, int32 vertical)
    : horizontal(horizontal), vertical(vertical) {}

bool Dpi::operator==(const Dpi& other) const {
  return horizontal == other.horizontal && vertical == other.vertical;
}

Media::Media()
    : type(CUSTOM_MEDIA),
      width_microns(0),
      height_microns(0),
      is_continuous_feed(false) {

}

Media::Media(MediaType type, int32 width_microns, int32 height_microns)
    : type(type),
      width_microns(width_microns),
      height_microns(height_microns),
      is_continuous_feed(width_microns <= 0 || height_microns <= 0) {
}

Media::Media(const std::string& custom_display_name, int32 width_microns,
             int32 height_microns)
    : type(CUSTOM_MEDIA),
      width_microns(width_microns),
      height_microns(height_microns),
      is_continuous_feed(width_microns <= 0 || height_microns <= 0),
      custom_display_name(custom_display_name) {
}

bool Media::IsValid() const {
  if (is_continuous_feed) {
    if (width_microns <= 0 && height_microns <= 0)
      return false;
  } else {
    if (width_microns <= 0 || height_microns <= 0)
      return false;
  }
  return true;
}

bool Media::operator==(const Media& other) const {
  return type == other.type &&
         width_microns == other.width_microns &&
         height_microns == other.height_microns &&
         is_continuous_feed == other.is_continuous_feed;
}

Interval::Interval() : start(0), end(0) {}

Interval::Interval(int32 start, int32 end)
    : start(start), end(end) {}

Interval::Interval(int32 start)
    : start(start), end(kMaxPageNumber) {}

bool Interval::operator==(const Interval& other) const {
  return start == other.start && end == other.end;
}

template<const char* kName>
class ItemsTraits {
 public:
  static std::string GetItemPath() {
    std::string result = kSectionPrinter;
    result += '.';
    result += kName;
    return result;
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
  static bool Load(const base::DictionaryValue& dict, ContentType* option) {
    return dict.GetString(kKeyContentType, option);
  }

  static void Save(ContentType option, base::DictionaryValue* dict) {
    dict->SetString(kKeyContentType, option);
  }
};

class ColorTraits : public ItemsTraits<kOptionColor> {
 public:
  static bool IsValid(const Color& option) {
    return option.IsValid();
  }

  static bool Load(const base::DictionaryValue& dict, Color* option) {
    std::string type_str;
    if (!dict.GetString(kKeyType, &type_str))
      return false;
    if (!TypeFromString(type_str, &option->type))
      return false;
    dict.GetString(kKeyVendorId, &option->vendor_id);
    dict.GetString(kCustomName, &option->custom_display_name);
    return true;
  }

  static void Save(const Color& option, base::DictionaryValue* dict) {
    dict->SetString(kKeyType, TypeToString(option.type));
    if (!option.vendor_id.empty())
      dict->SetString(kKeyVendorId, option.vendor_id);
    if (!option.custom_display_name.empty())
      dict->SetString(kCustomName, option.custom_display_name);
  }
};

class DuplexTraits : public NoValueValidation,
                     public ItemsTraits<kOptionDuplex> {
 public:
  static bool Load(const base::DictionaryValue& dict, DuplexType* option) {
    std::string type_str;
    return dict.GetString(kKeyType, &type_str) &&
           TypeFromString(type_str, option);
  }

  static void Save(DuplexType option, base::DictionaryValue* dict) {
    dict->SetString(kKeyType, TypeToString(option));
  }
};

class OrientationTraits : public NoValueValidation,
                          public ItemsTraits<kOptionPageOrientation> {
 public:
  static bool Load(const base::DictionaryValue& dict, OrientationType* option) {
    std::string type_str;
    return dict.GetString(kKeyType, &type_str) &&
           TypeFromString(type_str, option);
  }

  static void Save(OrientationType option, base::DictionaryValue* dict) {
    dict->SetString(kKeyType, TypeToString(option));
  }
};

class CopiesTraits : public ItemsTraits<kOptionCopies> {
 public:
  static bool IsValid(int32 option) {
    return option >= 1;
  }

  static bool Load(const base::DictionaryValue& dict, int32* option) {
    return dict.GetInteger(kOptionCopies, option);
  }

  static void Save(int32 option, base::DictionaryValue* dict) {
    dict->SetInteger(kOptionCopies, option);
  }
};

class MarginsTraits : public NoValueValidation,
                      public ItemsTraits<kOptionMargins> {
 public:
  static bool Load(const base::DictionaryValue& dict, Margins* option) {
    std::string type_str;
    if (!dict.GetString(kKeyType, &type_str))
      return false;
    if (!TypeFromString(type_str, &option->type))
      return false;
    return
        dict.GetInteger(kMargineTopMicrons, &option->top_microns) &&
        dict.GetInteger(kMargineRightMicrons, &option->right_microns) &&
        dict.GetInteger(kMargineBottomMicrons, &option->bottom_microns) &&
        dict.GetInteger(kMargineLeftMicrons, &option->left_microns);
  }

  static void Save(const Margins& option, base::DictionaryValue* dict) {
    dict->SetString(kKeyType, TypeToString(option.type));
    dict->SetInteger(kMargineTopMicrons, option.top_microns);
    dict->SetInteger(kMargineRightMicrons, option.right_microns);
    dict->SetInteger(kMargineBottomMicrons, option.bottom_microns);
    dict->SetInteger(kMargineLeftMicrons, option.left_microns);
  }
};

class DpiTraits : public ItemsTraits<kOptionDpi> {
 public:
  static bool IsValid(const Dpi& option) {
    return option.horizontal && option.vertical > 0;
  }

  static bool Load(const base::DictionaryValue& dict, Dpi* option) {
    if (!dict.GetInteger(kDpiHorizontal, &option->horizontal) ||
        !dict.GetInteger(kDpiVertical, &option->vertical)) {
      return false;
    }
    return true;
  }

  static void Save(const Dpi& option, base::DictionaryValue* dict) {
    dict->SetInteger(kDpiHorizontal, option.horizontal);
    dict->SetInteger(kDpiVertical, option.vertical);
  }
};

class FitToPageTraits : public NoValueValidation,
                        public ItemsTraits<kOptionFitToPage> {
 public:
  static bool Load(const base::DictionaryValue& dict, FitToPageType* option) {
    std::string type_str;
    return dict.GetString(kKeyType, &type_str) &&
           TypeFromString(type_str, option);
  }

  static void Save(FitToPageType option, base::DictionaryValue* dict) {
    dict->SetString(kKeyType, TypeToString(option));
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

  static bool Load(const base::DictionaryValue& dict, PageRange* option) {
    const base::ListValue* list = NULL;
    if (!dict.GetList(kPageRangeInterval, &list))
      return false;
    for (size_t i = 0; i < list->GetSize(); ++i) {
      const base::DictionaryValue* interval = NULL;
      if (!list->GetDictionary(i, &interval))
        return false;
      Interval new_interval(1, kMaxPageNumber);
      interval->GetInteger(kPageRangeStart, &new_interval.start);
      interval->GetInteger(kPageRangeEnd, &new_interval.end);
      option->push_back(new_interval);
    }
    return true;
  }

  static void Save(const PageRange& option, base::DictionaryValue* dict) {
    if (!option.empty()) {
      base::ListValue* list = new base::ListValue;
      dict->Set(kPageRangeInterval, list);
      for (size_t i = 0; i < option.size(); ++i) {
        base::DictionaryValue* interval = new base::DictionaryValue;
        list->Append(interval);
        interval->SetInteger(kPageRangeStart, option[i].start);
        if (option[i].end < kMaxPageNumber)
          interval->SetInteger(kPageRangeEnd, option[i].end);
      }
    }
  }
};

class MediaTraits : public ItemsTraits<kOptionMediaSize> {
 public:
  static bool IsValid(const Media& option) {
    return option.IsValid();
  }

  static bool Load(const base::DictionaryValue& dict, Media* option) {
    std::string type_str;
    if (dict.GetString(kKeyName, &type_str)) {
      if (!TypeFromString(type_str, &option->type))
        return false;
    }

    dict.GetInteger(kMediaWidth, &option->width_microns);
    dict.GetInteger(kMediaHeight, &option->height_microns);
    dict.GetBoolean(kMediaIsContinuous, &option->is_continuous_feed);
    dict.GetString(kCustomName, &option->custom_display_name);
    return true;
  }

  static void Save(const Media& option, base::DictionaryValue* dict) {
    if (option.type != CUSTOM_MEDIA)
      dict->SetString(kKeyName, TypeToString(option.type));
    if (!option.custom_display_name.empty())
      dict->SetString(kCustomName, option.custom_display_name);
    if (option.width_microns > 0)
      dict->SetInteger(kMediaWidth, option.width_microns);
    if (option.height_microns > 0)
      dict->SetInteger(kMediaHeight, option.height_microns);
    if (option.is_continuous_feed)
      dict->SetBoolean(kMediaIsContinuous, true);
  }
};

class CollateTraits : public NoValueValidation,
                      public ItemsTraits<kOptionCollate> {
 public:
  static const bool kDefault = true;

  static bool Load(const base::DictionaryValue& dict, bool* option) {
    return dict.GetBoolean(kOptionCollate, option);
  }

  static void Save(bool option, base::DictionaryValue* dict) {
    dict->SetBoolean(kOptionCollate, option);
  }
};

class ReverseTraits : public NoValueValidation,
                      public ItemsTraits<kOptionReverse> {
 public:
  static const bool kDefault = false;

  static bool Load(const base::DictionaryValue& dict, bool* option) {
    return dict.GetBoolean(kOptionReverse, option);
  }

  static void Save(bool option, base::DictionaryValue* dict) {
    dict->SetBoolean(kOptionReverse, option);
  }
};

}  // namespace printer

using namespace printer;

template class ListCapability<ContentType, ContentTypeTraits>;
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

template class TicketItem<Color, ColorTraits>;
template class TicketItem<DuplexType, DuplexTraits>;
template class TicketItem<OrientationType, OrientationTraits>;
template class TicketItem<Margins, MarginsTraits>;
template class TicketItem<Dpi, DpiTraits>;
template class TicketItem<FitToPageType, FitToPageTraits>;
template class TicketItem<Media, MediaTraits>;
template class TicketItem<int32, CopiesTraits>;
template class TicketItem<PageRange, PageRangeTraits>;
template class TicketItem<bool, CollateTraits>;
template class TicketItem<bool, ReverseTraits>;

}  // namespace cloud_devices
