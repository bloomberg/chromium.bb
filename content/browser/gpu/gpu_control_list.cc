// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_control_list.h"

#include "base/cpu.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"
#include "content/browser/gpu/gpu_util.h"
#include "content/public/common/gpu_info.h"

namespace content {
namespace {

// Break a version string into segments.  Return true if each segment is
// a valid number.
bool ProcessVersionString(const std::string& version_string,
                          char splitter,
                          std::vector<std::string>* version) {
  DCHECK(version);
  base::SplitString(version_string, splitter, version);
  if (version->size() == 0)
    return false;
  // If the splitter is '-', we assume it's a date with format "mm-dd-yyyy";
  // we split it into the order of "yyyy", "mm", "dd".
  if (splitter == '-') {
    std::string year = (*version)[version->size() - 1];
    for (int i = version->size() - 1; i > 0; --i) {
      (*version)[i] = (*version)[i - 1];
    }
    (*version)[0] = year;
  }
  for (size_t i = 0; i < version->size(); ++i) {
    unsigned num = 0;
    if (!base::StringToUint((*version)[i], &num))
      return false;
  }
  return true;
}

// Compare two number strings using numerical ordering.
// Return  0 if number = number_ref,
//         1 if number > number_ref,
//        -1 if number < number_ref.
int CompareNumericalNumberStrings(
    const std::string& number, const std::string& number_ref) {
  unsigned value1 = 0;
  unsigned value2 = 0;
  bool valid = base::StringToUint(number, &value1);
  DCHECK(valid);
  valid = base::StringToUint(number_ref, &value2);
  DCHECK(valid);
  if (value1 == value2)
    return 0;
  if (value1 > value2)
    return 1;
  return -1;
}

// Compare two number strings using lexical ordering.
// Return  0 if number = number_ref,
//         1 if number > number_ref,
//        -1 if number < number_ref.
// We only compare as many digits as number_ref contains.
// If number_ref is xxx, it's considered as xxx*
// For example: CompareLexicalNumberStrings("121", "12") returns 0,
//              CompareLexicalNumberStrings("12", "121") returns -1.
int CompareLexicalNumberStrings(
    const std::string& number, const std::string& number_ref) {
  for (size_t i = 0; i < number_ref.length(); ++i) {
    unsigned value1 = 0;
    if (i < number.length())
      value1 = number[i] - '0';
    unsigned value2 = number_ref[i] - '0';
    if (value1 > value2)
      return 1;
    if (value1 < value2)
      return -1;
  }
  return 0;
}

bool GpuUnmatched(uint32 vendor_id, const std::vector<uint32>& device_id_list,
                  const GPUInfo::GPUDevice& gpu) {
  if (vendor_id == 0)
    return false;
  if (vendor_id != gpu.vendor_id)
    return true;
  bool device_specified = false;
  for (size_t i = 0; i < device_id_list.size(); ++i) {
    if (device_id_list[i] == 0)
      continue;
    if (device_id_list[i] == gpu.device_id)
      return false;
    device_specified = true;
  }
  return device_specified;
}

const char kMultiGpuStyleStringAMDSwitchable[] = "amd_switchable";
const char kMultiGpuStyleStringOptimus[] = "optimus";

const char kMultiGpuCategoryStringPrimary[] = "primary";
const char kMultiGpuCategoryStringSecondary[] = "secondary";
const char kMultiGpuCategoryStringAny[] = "any";

const char kVersionStyleStringNumerical[] = "numerical";
const char kVersionStyleStringLexical[] = "lexical";

const char kOp[] = "op";

}  // namespace anonymous

GpuControlList::VersionInfo::VersionInfo(
    const std::string& version_op,
    const std::string& version_style,
    const std::string& version_string,
    const std::string& version_string2)
    : version_style_(kVersionStyleNumerical) {
  op_ = StringToNumericOp(version_op);
  if (op_ == kUnknown || op_ == kAny)
    return;
  version_style_ = StringToVersionStyle(version_style);
  if (!ProcessVersionString(version_string, '.', &version_)) {
    op_ = kUnknown;
    return;
  }
  if (op_ == kBetween) {
    if (!ProcessVersionString(version_string2, '.', &version2_))
      op_ = kUnknown;
  }
}

GpuControlList::VersionInfo::~VersionInfo() {
}

bool GpuControlList::VersionInfo::Contains(
    const std::string& version_string) const {
  return Contains(version_string, '.');
}

bool GpuControlList::VersionInfo::Contains(
    const std::string& version_string, char splitter) const {
  if (op_ == kUnknown)
    return false;
  if (op_ == kAny)
    return true;
  std::vector<std::string> version;
  if (!ProcessVersionString(version_string, splitter, &version))
    return false;
  int relation = Compare(version, version_, version_style_);
  if (op_ == kEQ)
    return (relation == 0);
  else if (op_ == kLT)
    return (relation < 0);
  else if (op_ == kLE)
    return (relation <= 0);
  else if (op_ == kGT)
    return (relation > 0);
  else if (op_ == kGE)
    return (relation >= 0);
  // op_ == kBetween
  if (relation < 0)
    return false;
  return Compare(version, version2_, version_style_) <= 0;
}

bool GpuControlList::VersionInfo::IsValid() const {
  return (op_ != kUnknown && version_style_ != kVersionStyleUnknown);
}

bool GpuControlList::VersionInfo::IsLexical() const {
  return version_style_ == kVersionStyleLexical;
}

// static
int GpuControlList::VersionInfo::Compare(
    const std::vector<std::string>& version,
    const std::vector<std::string>& version_ref,
    VersionStyle version_style) {
  DCHECK(version.size() > 0 && version_ref.size() > 0);
  DCHECK(version_style != kVersionStyleUnknown);
  for (size_t i = 0; i < version_ref.size(); ++i) {
    if (i >= version.size())
      return 0;
    int ret = 0;
    // We assume both versions are checked by ProcessVersionString().
    if (i > 0 && version_style == kVersionStyleLexical)
      ret = CompareLexicalNumberStrings(version[i], version_ref[i]);
    else
      ret = CompareNumericalNumberStrings(version[i], version_ref[i]);
    if (ret != 0)
      return ret;
  }
  return 0;
}

// static
GpuControlList::VersionInfo::VersionStyle
GpuControlList::VersionInfo::StringToVersionStyle(
    const std::string& version_style) {
  if (version_style.empty() || version_style == kVersionStyleStringNumerical)
    return kVersionStyleNumerical;
  if (version_style == kVersionStyleStringLexical)
    return kVersionStyleLexical;
  return kVersionStyleUnknown;
}

GpuControlList::OsInfo::OsInfo(const std::string& os,
                             const std::string& version_op,
                             const std::string& version_string,
                             const std::string& version_string2) {
  type_ = StringToOsType(os);
  if (type_ != kOsUnknown) {
    version_info_.reset(
        new VersionInfo(version_op, "", version_string, version_string2));
  }
}

GpuControlList::OsInfo::~OsInfo() {}

bool GpuControlList::OsInfo::Contains(OsType type,
                                    const std::string& version) const {
  if (!IsValid())
    return false;
  if (type_ != type && type_ != kOsAny)
    return false;
  return version_info_->Contains(version);
}

bool GpuControlList::OsInfo::IsValid() const {
  return type_ != kOsUnknown && version_info_->IsValid();
}

GpuControlList::OsType GpuControlList::OsInfo::type() const {
  return type_;
}

GpuControlList::OsType GpuControlList::OsInfo::StringToOsType(
    const std::string& os) {
  if (os == "win")
    return kOsWin;
  else if (os == "macosx")
    return kOsMacosx;
  else if (os == "android")
    return kOsAndroid;
  else if (os == "linux")
    return kOsLinux;
  else if (os == "chromeos")
    return kOsChromeOS;
  else if (os == "any")
    return kOsAny;
  return kOsUnknown;
}

GpuControlList::MachineModelInfo::MachineModelInfo(
    const std::string& name_op,
    const std::string& name_value,
    const std::string& version_op,
    const std::string& version_string,
    const std::string& version_string2) {
  name_info_.reset(new StringInfo(name_op, name_value));
  version_info_.reset(
      new VersionInfo(version_op, "", version_string, version_string2));
}

GpuControlList::MachineModelInfo::~MachineModelInfo() {}

bool GpuControlList::MachineModelInfo::Contains(
    const std::string& name, const std::string& version) const {
  if (!IsValid())
    return false;
  if (!name_info_->Contains(name))
    return false;
  return version_info_->Contains(version);
}

bool GpuControlList::MachineModelInfo::IsValid() const {
  return name_info_->IsValid() && version_info_->IsValid();
}

GpuControlList::StringInfo::StringInfo(const std::string& string_op,
                                     const std::string& string_value) {
  op_ = StringToOp(string_op);
  value_ = StringToLowerASCII(string_value);
}

bool GpuControlList::StringInfo::Contains(const std::string& value) const {
  std::string my_value = StringToLowerASCII(value);
  switch (op_) {
    case kContains:
      return strstr(my_value.c_str(), value_.c_str()) != NULL;
    case kBeginWith:
      return StartsWithASCII(my_value, value_, false);
    case kEndWith:
      return EndsWith(my_value, value_, false);
    case kEQ:
      return value_ == my_value;
    default:
      return false;
  }
}

bool GpuControlList::StringInfo::IsValid() const {
  return op_ != kUnknown;
}

GpuControlList::StringInfo::Op GpuControlList::StringInfo::StringToOp(
    const std::string& string_op) {
  if (string_op == "=")
    return kEQ;
  else if (string_op == "contains")
    return kContains;
  else if (string_op == "beginwith")
    return kBeginWith;
  else if (string_op == "endwith")
    return kEndWith;
  return kUnknown;
}

GpuControlList::FloatInfo::FloatInfo(const std::string& float_op,
                                     const std::string& float_value,
                                     const std::string& float_value2)
    : op_(kUnknown),
      value_(0.f),
      value2_(0.f) {
  double dvalue = 0;
  if (!base::StringToDouble(float_value, &dvalue)) {
    op_ = kUnknown;
    return;
  }
  value_ = static_cast<float>(dvalue);
  op_ = StringToNumericOp(float_op);
  if (op_ == kBetween) {
    if (!base::StringToDouble(float_value2, &dvalue)) {
      op_ = kUnknown;
      return;
    }
    value2_ = static_cast<float>(dvalue);
  }
}

bool GpuControlList::FloatInfo::Contains(float value) const {
  if (op_ == kUnknown)
    return false;
  if (op_ == kAny)
    return true;
  if (op_ == kEQ)
    return (value == value_);
  if (op_ == kLT)
    return (value < value_);
  if (op_ == kLE)
    return (value <= value_);
  if (op_ == kGT)
    return (value > value_);
  if (op_ == kGE)
    return (value >= value_);
  DCHECK(op_ == kBetween);
  return ((value_ <= value && value <= value2_) ||
          (value2_ <= value && value <= value_));
}

bool GpuControlList::FloatInfo::IsValid() const {
  return op_ != kUnknown;
}

GpuControlList::IntInfo::IntInfo(const std::string& int_op,
                                 const std::string& int_value,
                                 const std::string& int_value2)
    : op_(kUnknown),
      value_(0),
      value2_(0) {
  if (!base::StringToInt(int_value, &value_)) {
    op_ = kUnknown;
    return;
  }
  op_ = StringToNumericOp(int_op);
  if (op_ == kBetween &&
      !base::StringToInt(int_value2, &value2_))
    op_ = kUnknown;
}

bool GpuControlList::IntInfo::Contains(int value) const {
  if (op_ == kUnknown)
    return false;
  if (op_ == kAny)
    return true;
  if (op_ == kEQ)
    return (value == value_);
  if (op_ == kLT)
    return (value < value_);
  if (op_ == kLE)
    return (value <= value_);
  if (op_ == kGT)
    return (value > value_);
  if (op_ == kGE)
    return (value >= value_);
  DCHECK(op_ == kBetween);
  return ((value_ <= value && value <= value2_) ||
          (value2_ <= value && value <= value_));
}

bool GpuControlList::IntInfo::IsValid() const {
  return op_ != kUnknown;
}

// static
GpuControlList::ScopedGpuControlListEntry
GpuControlList::GpuControlListEntry::GetEntryFromValue(
    const base::DictionaryValue* value, bool top_level,
    const FeatureMap& feature_map) {
  DCHECK(value);
  ScopedGpuControlListEntry entry(new GpuControlListEntry());

  size_t dictionary_entry_count = 0;

  if (top_level) {
    uint32 id;
    if (!value->GetInteger("id", reinterpret_cast<int*>(&id)) ||
        !entry->SetId(id)) {
      LOG(WARNING) << "Malformed id entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;

    bool disabled;
    if (value->GetBoolean("disabled", &disabled)) {
      entry->SetDisabled(disabled);
      dictionary_entry_count++;
    }
  }

  std::string description;
  if (value->GetString("description", &description)) {
    entry->description_ = description;
    dictionary_entry_count++;
  } else {
    entry->description_ = "The GPU is unavailable for an unexplained reason.";
  }

  const base::ListValue* cr_bugs;
  if (value->GetList("cr_bugs", &cr_bugs)) {
    for (size_t i = 0; i < cr_bugs->GetSize(); ++i) {
      int bug_id;
      if (cr_bugs->GetInteger(i, &bug_id)) {
        entry->cr_bugs_.push_back(bug_id);
      } else {
        LOG(WARNING) << "Malformed cr_bugs entry " << entry->id();
        return NULL;
      }
    }
    dictionary_entry_count++;
  }

  const base::ListValue* webkit_bugs;
  if (value->GetList("webkit_bugs", &webkit_bugs)) {
    for (size_t i = 0; i < webkit_bugs->GetSize(); ++i) {
      int bug_id;
      if (webkit_bugs->GetInteger(i, &bug_id)) {
        entry->webkit_bugs_.push_back(bug_id);
      } else {
        LOG(WARNING) << "Malformed webkit_bugs entry " << entry->id();
        return NULL;
      }
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* os_value = NULL;
  if (value->GetDictionary("os", &os_value)) {
    std::string os_type;
    std::string os_version_op = "any";
    std::string os_version_string;
    std::string os_version_string2;
    os_value->GetString("type", &os_type);
    const base::DictionaryValue* os_version_value = NULL;
    if (os_value->GetDictionary("version", &os_version_value)) {
      os_version_value->GetString(kOp, &os_version_op);
      os_version_value->GetString("number", &os_version_string);
      os_version_value->GetString("number2", &os_version_string2);
    }
    if (!entry->SetOsInfo(os_type, os_version_op, os_version_string,
                          os_version_string2)) {
      LOG(WARNING) << "Malformed os entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  std::string vendor_id;
  if (value->GetString("vendor_id", &vendor_id)) {
    if (!entry->SetVendorId(vendor_id)) {
      LOG(WARNING) << "Malformed vendor_id entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::ListValue* device_id_list;
  if (value->GetList("device_id", &device_id_list)) {
    for (size_t i = 0; i < device_id_list->GetSize(); ++i) {
        std::string device_id;
      if (!device_id_list->GetString(i, &device_id) ||
          !entry->AddDeviceId(device_id)) {
        LOG(WARNING) << "Malformed device_id entry " << entry->id();
        return NULL;
      }
    }
    dictionary_entry_count++;
  }

  std::string multi_gpu_style;
  if (value->GetString("multi_gpu_style", &multi_gpu_style)) {
    if (!entry->SetMultiGpuStyle(multi_gpu_style)) {
      LOG(WARNING) << "Malformed multi_gpu_style entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  std::string multi_gpu_category;
  if (value->GetString("multi_gpu_category", &multi_gpu_category)) {
    if (!entry->SetMultiGpuCategory(multi_gpu_category)) {
      LOG(WARNING) << "Malformed multi_gpu_category entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* driver_vendor_value = NULL;
  if (value->GetDictionary("driver_vendor", &driver_vendor_value)) {
    std::string vendor_op;
    std::string vendor_value;
    driver_vendor_value->GetString(kOp, &vendor_op);
    driver_vendor_value->GetString("value", &vendor_value);
    if (!entry->SetDriverVendorInfo(vendor_op, vendor_value)) {
      LOG(WARNING) << "Malformed driver_vendor entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* driver_version_value = NULL;
  if (value->GetDictionary("driver_version", &driver_version_value)) {
    std::string driver_version_op = "any";
    std::string driver_version_style;
    std::string driver_version_string;
    std::string driver_version_string2;
    driver_version_value->GetString(kOp, &driver_version_op);
    driver_version_value->GetString("style", &driver_version_style);
    driver_version_value->GetString("number", &driver_version_string);
    driver_version_value->GetString("number2", &driver_version_string2);
    if (!entry->SetDriverVersionInfo(driver_version_op,
                                     driver_version_style,
                                     driver_version_string,
                                     driver_version_string2)) {
      LOG(WARNING) << "Malformed driver_version entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* driver_date_value = NULL;
  if (value->GetDictionary("driver_date", &driver_date_value)) {
    std::string driver_date_op = "any";
    std::string driver_date_string;
    std::string driver_date_string2;
    driver_date_value->GetString(kOp, &driver_date_op);
    driver_date_value->GetString("number", &driver_date_string);
    driver_date_value->GetString("number2", &driver_date_string2);
    if (!entry->SetDriverDateInfo(driver_date_op, driver_date_string,
                                  driver_date_string2)) {
      LOG(WARNING) << "Malformed driver_date entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* gl_vendor_value = NULL;
  if (value->GetDictionary("gl_vendor", &gl_vendor_value)) {
    std::string vendor_op;
    std::string vendor_value;
    gl_vendor_value->GetString(kOp, &vendor_op);
    gl_vendor_value->GetString("value", &vendor_value);
    if (!entry->SetGLVendorInfo(vendor_op, vendor_value)) {
      LOG(WARNING) << "Malformed gl_vendor entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* gl_renderer_value = NULL;
  if (value->GetDictionary("gl_renderer", &gl_renderer_value)) {
    std::string renderer_op;
    std::string renderer_value;
    gl_renderer_value->GetString(kOp, &renderer_op);
    gl_renderer_value->GetString("value", &renderer_value);
    if (!entry->SetGLRendererInfo(renderer_op, renderer_value)) {
      LOG(WARNING) << "Malformed gl_renderer entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* cpu_brand_value = NULL;
  if (value->GetDictionary("cpu_info", &cpu_brand_value)) {
    std::string cpu_op;
    std::string cpu_value;
    cpu_brand_value->GetString(kOp, &cpu_op);
    cpu_brand_value->GetString("value", &cpu_value);
    if (!entry->SetCpuBrand(cpu_op, cpu_value)) {
      LOG(WARNING) << "Malformed cpu_brand entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* perf_graphics_value = NULL;
  if (value->GetDictionary("perf_graphics", &perf_graphics_value)) {
    std::string op;
    std::string float_value;
    std::string float_value2;
    perf_graphics_value->GetString(kOp, &op);
    perf_graphics_value->GetString("value", &float_value);
    perf_graphics_value->GetString("value2", &float_value2);
    if (!entry->SetPerfGraphicsInfo(op, float_value, float_value2)) {
      LOG(WARNING) << "Malformed perf_graphics entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* perf_gaming_value = NULL;
  if (value->GetDictionary("perf_gaming", &perf_gaming_value)) {
    std::string op;
    std::string float_value;
    std::string float_value2;
    perf_gaming_value->GetString(kOp, &op);
    perf_gaming_value->GetString("value", &float_value);
    perf_gaming_value->GetString("value2", &float_value2);
    if (!entry->SetPerfGamingInfo(op, float_value, float_value2)) {
      LOG(WARNING) << "Malformed perf_gaming entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* perf_overall_value = NULL;
  if (value->GetDictionary("perf_overall", &perf_overall_value)) {
    std::string op;
    std::string float_value;
    std::string float_value2;
    perf_overall_value->GetString(kOp, &op);
    perf_overall_value->GetString("value", &float_value);
    perf_overall_value->GetString("value2", &float_value2);
    if (!entry->SetPerfOverallInfo(op, float_value, float_value2)) {
      LOG(WARNING) << "Malformed perf_overall entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* machine_model_value = NULL;
  if (value->GetDictionary("machine_model", &machine_model_value)) {
    std::string name_op;
    std::string name_value;
    const base::DictionaryValue* name = NULL;
    if (machine_model_value->GetDictionary("name", &name)) {
      name->GetString(kOp, &name_op);
      name->GetString("value", &name_value);
    }

    std::string version_op = "any";
    std::string version_string;
    std::string version_string2;
    const base::DictionaryValue* version_value = NULL;
    if (machine_model_value->GetDictionary("version", &version_value)) {
      version_value->GetString(kOp, &version_op);
      version_value->GetString("number", &version_string);
      version_value->GetString("number2", &version_string2);
    }
    if (!entry->SetMachineModelInfo(
            name_op, name_value, version_op, version_string, version_string2)) {
      LOG(WARNING) << "Malformed machine_model entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  const base::DictionaryValue* gpu_count_value = NULL;
  if (value->GetDictionary("gpu_count", &gpu_count_value)) {
    std::string op;
    std::string int_value;
    std::string int_value2;
    gpu_count_value->GetString(kOp, &op);
    gpu_count_value->GetString("value", &int_value);
    gpu_count_value->GetString("value2", &int_value2);
    if (!entry->SetGpuCountInfo(op, int_value, int_value2)) {
      LOG(WARNING) << "Malformed gpu_count entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  if (top_level) {
    const base::ListValue* feature_value = NULL;
    if (value->GetList("features", &feature_value)) {
      std::vector<std::string> feature_list;
      for (size_t i = 0; i < feature_value->GetSize(); ++i) {
        std::string feature;
        if (feature_value->GetString(i, &feature)) {
          feature_list.push_back(feature);
        } else {
          LOG(WARNING) << "Malformed feature entry " << entry->id();
          return NULL;
        }
      }
      if (!entry->SetFeatures(feature_list, feature_map)) {
        LOG(WARNING) << "Malformed feature entry " << entry->id();
        return NULL;
      }
      dictionary_entry_count++;
    }
  }

  if (top_level) {
    const base::ListValue* exception_list_value = NULL;
    if (value->GetList("exceptions", &exception_list_value)) {
      for (size_t i = 0; i < exception_list_value->GetSize(); ++i) {
        const base::DictionaryValue* exception_value = NULL;
        if (!exception_list_value->GetDictionary(i, &exception_value)) {
          LOG(WARNING) << "Malformed exceptions entry " << entry->id();
          return NULL;
        }
        ScopedGpuControlListEntry exception(
            GetEntryFromValue(exception_value, false, feature_map));
        if (exception == NULL) {
          LOG(WARNING) << "Malformed exceptions entry " << entry->id();
          return NULL;
        }
        if (exception->contains_unknown_fields_) {
          LOG(WARNING) << "Exception with unknown fields " << entry->id();
          entry->contains_unknown_fields_ = true;
        } else {
          entry->AddException(exception);
        }
      }
      dictionary_entry_count++;
    }

    const base::DictionaryValue* browser_version_value = NULL;
    // browser_version is processed in LoadGpuControlList().
    if (value->GetDictionary("browser_version", &browser_version_value))
      dictionary_entry_count++;
  }

  if (value->size() != dictionary_entry_count) {
    LOG(WARNING) << "Entry with unknown fields " << entry->id();
    entry->contains_unknown_fields_ = true;
  }
  return entry;
}

GpuControlList::GpuControlListEntry::GpuControlListEntry()
    : id_(0),
      disabled_(false),
      vendor_id_(0),
      multi_gpu_style_(kMultiGpuStyleNone),
      multi_gpu_category_(kMultiGpuCategoryPrimary),
      contains_unknown_fields_(false),
      contains_unknown_features_(false) {
}

GpuControlList::GpuControlListEntry::~GpuControlListEntry() { }

bool GpuControlList::GpuControlListEntry::SetId(uint32 id) {
  if (id != 0) {
    id_ = id;
    return true;
  }
  return false;
}

void GpuControlList::GpuControlListEntry::SetDisabled(bool disabled) {
  disabled_ = disabled;
}

bool GpuControlList::GpuControlListEntry::SetOsInfo(
    const std::string& os,
    const std::string& version_op,
    const std::string& version_string,
    const std::string& version_string2) {
  os_info_.reset(new OsInfo(os, version_op, version_string, version_string2));
  return os_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetVendorId(
    const std::string& vendor_id_string) {
  vendor_id_ = 0;
  return base::HexStringToInt(vendor_id_string,
                              reinterpret_cast<int*>(&vendor_id_));
}

bool GpuControlList::GpuControlListEntry::AddDeviceId(
    const std::string& device_id_string) {
  uint32 device_id = 0;
  if (base::HexStringToInt(device_id_string,
                           reinterpret_cast<int*>(&device_id))) {
    device_id_list_.push_back(device_id);
    return true;
  }
  return false;
}

bool GpuControlList::GpuControlListEntry::SetMultiGpuStyle(
    const std::string& multi_gpu_style_string) {
  MultiGpuStyle style = StringToMultiGpuStyle(multi_gpu_style_string);
  if (style == kMultiGpuStyleNone)
    return false;
  multi_gpu_style_ = style;
  return true;
}

bool GpuControlList::GpuControlListEntry::SetMultiGpuCategory(
    const std::string& multi_gpu_category_string) {
  MultiGpuCategory category =
      StringToMultiGpuCategory(multi_gpu_category_string);
  if (category == kMultiGpuCategoryNone)
    return false;
  multi_gpu_category_ = category;
  return true;
}

bool GpuControlList::GpuControlListEntry::SetDriverVendorInfo(
    const std::string& vendor_op,
    const std::string& vendor_value) {
  driver_vendor_info_.reset(new StringInfo(vendor_op, vendor_value));
  return driver_vendor_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetDriverVersionInfo(
    const std::string& version_op,
    const std::string& version_style,
    const std::string& version_string,
    const std::string& version_string2) {
  driver_version_info_.reset(new VersionInfo(
      version_op, version_style, version_string, version_string2));
  return driver_version_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetDriverDateInfo(
    const std::string& date_op,
    const std::string& date_string,
    const std::string& date_string2) {
  driver_date_info_.reset(
      new VersionInfo(date_op, "", date_string, date_string2));
  return driver_date_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetGLVendorInfo(
    const std::string& vendor_op,
    const std::string& vendor_value) {
  gl_vendor_info_.reset(new StringInfo(vendor_op, vendor_value));
  return gl_vendor_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetGLRendererInfo(
    const std::string& renderer_op,
    const std::string& renderer_value) {
  gl_renderer_info_.reset(new StringInfo(renderer_op, renderer_value));
  return gl_renderer_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetCpuBrand(
    const std::string& cpu_op,
    const std::string& cpu_value) {
  cpu_brand_.reset(new StringInfo(cpu_op, cpu_value));
  return cpu_brand_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetPerfGraphicsInfo(
    const std::string& op,
    const std::string& float_string,
    const std::string& float_string2) {
  perf_graphics_info_.reset(new FloatInfo(op, float_string, float_string2));
  return perf_graphics_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetPerfGamingInfo(
    const std::string& op,
    const std::string& float_string,
    const std::string& float_string2) {
  perf_gaming_info_.reset(new FloatInfo(op, float_string, float_string2));
  return perf_gaming_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetPerfOverallInfo(
    const std::string& op,
    const std::string& float_string,
    const std::string& float_string2) {
  perf_overall_info_.reset(new FloatInfo(op, float_string, float_string2));
  return perf_overall_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetMachineModelInfo(
    const std::string& name_op,
    const std::string& name_value,
    const std::string& version_op,
    const std::string& version_string,
    const std::string& version_string2) {
  machine_model_info_.reset(new MachineModelInfo(
      name_op, name_value, version_op, version_string, version_string2));
  return machine_model_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetGpuCountInfo(
    const std::string& op,
    const std::string& int_string,
    const std::string& int_string2) {
  gpu_count_info_.reset(new IntInfo(op, int_string, int_string2));
  return gpu_count_info_->IsValid();
}

bool GpuControlList::GpuControlListEntry::SetFeatures(
    const std::vector<std::string>& feature_strings,
    const FeatureMap& feature_map) {
  size_t size = feature_strings.size();
  if (size == 0)
    return false;
  int features = 0;
  for (size_t i = 0; i < size; ++i) {
    int feature = 0;
    bool valid = StringToFeature(feature_strings[i], &feature, feature_map);
    if (valid)
        features |= feature;
    else
        contains_unknown_features_ = true;
  }
  features_ = features;
  return true;
}

void GpuControlList::GpuControlListEntry::AddException(
    ScopedGpuControlListEntry exception) {
  exceptions_.push_back(exception);
}

// static
GpuControlList::GpuControlListEntry::MultiGpuStyle
GpuControlList::GpuControlListEntry::StringToMultiGpuStyle(
    const std::string& style) {
  if (style == kMultiGpuStyleStringOptimus)
    return kMultiGpuStyleOptimus;
  if (style == kMultiGpuStyleStringAMDSwitchable)
    return kMultiGpuStyleAMDSwitchable;
  return kMultiGpuStyleNone;
}

// static
GpuControlList::GpuControlListEntry::MultiGpuCategory
GpuControlList::GpuControlListEntry::StringToMultiGpuCategory(
    const std::string& category) {
  if (category == kMultiGpuCategoryStringPrimary)
    return kMultiGpuCategoryPrimary;
  if (category == kMultiGpuCategoryStringSecondary)
    return kMultiGpuCategorySecondary;
  if (category == kMultiGpuCategoryStringAny)
    return kMultiGpuCategoryAny;
  return kMultiGpuCategoryNone;
}

bool GpuControlList::GpuControlListEntry::Contains(
    OsType os_type, const std::string& os_version,
    const GPUInfo& gpu_info) const {
  DCHECK(os_type != kOsAny);
  if (os_info_.get() != NULL && !os_info_->Contains(os_type, os_version))
    return false;
  bool is_not_primary_gpu =
      GpuUnmatched(vendor_id_, device_id_list_, gpu_info.gpu);
  bool is_not_secondary_gpu = true;
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    is_not_secondary_gpu = is_not_secondary_gpu &&
        GpuUnmatched(vendor_id_, device_id_list_, gpu_info.secondary_gpus[i]);
  }
  switch (multi_gpu_category_) {
    case kMultiGpuCategoryPrimary:
      if (is_not_primary_gpu)
        return false;
      break;
    case kMultiGpuCategorySecondary:
      if (is_not_secondary_gpu)
        return false;
      break;
    case kMultiGpuCategoryAny:
      if (is_not_primary_gpu && is_not_secondary_gpu)
        return false;
      break;
    case kMultiGpuCategoryNone:
      break;
  }
  switch (multi_gpu_style_) {
    case kMultiGpuStyleOptimus:
      if (!gpu_info.optimus)
        return false;
      break;
    case kMultiGpuStyleAMDSwitchable:
      if (!gpu_info.amd_switchable)
        return false;
      break;
    case kMultiGpuStyleNone:
      break;
  }
  if (driver_vendor_info_.get() != NULL && !gpu_info.driver_vendor.empty() &&
      !driver_vendor_info_->Contains(gpu_info.driver_vendor))
    return false;
  if (driver_version_info_.get() != NULL && !gpu_info.driver_version.empty()) {
    if (!driver_version_info_->Contains(gpu_info.driver_version))
      return false;
  }
  if (driver_date_info_.get() != NULL && !gpu_info.driver_date.empty()) {
    if (!driver_date_info_->Contains(gpu_info.driver_date, '-'))
      return false;
  }
  if (gl_vendor_info_.get() != NULL && !gpu_info.gl_vendor.empty() &&
      !gl_vendor_info_->Contains(gpu_info.gl_vendor))
    return false;
  if (gl_renderer_info_.get() != NULL && !gpu_info.gl_renderer.empty() &&
      !gl_renderer_info_->Contains(gpu_info.gl_renderer))
    return false;
  if (perf_graphics_info_.get() != NULL &&
      (gpu_info.performance_stats.graphics == 0.0 ||
       !perf_graphics_info_->Contains(gpu_info.performance_stats.graphics)))
    return false;
  if (perf_gaming_info_.get() != NULL &&
      (gpu_info.performance_stats.gaming == 0.0 ||
       !perf_gaming_info_->Contains(gpu_info.performance_stats.gaming)))
    return false;
  if (perf_overall_info_.get() != NULL &&
      (gpu_info.performance_stats.overall == 0.0 ||
       !perf_overall_info_->Contains(gpu_info.performance_stats.overall)))
    return false;
  if (machine_model_info_.get() != NULL) {
    std::vector<std::string> name_version;
    base::SplitString(gpu_info.machine_model, ' ', &name_version);
    if (name_version.size() == 2 &&
        !machine_model_info_->Contains(name_version[0], name_version[1]))
      return false;
  }
  if (gpu_count_info_.get() != NULL &&
      !gpu_count_info_->Contains(gpu_info.secondary_gpus.size() + 1))
    return false;
  if (cpu_brand_.get() != NULL) {
    base::CPU cpu_info;
    if (!cpu_brand_->Contains(cpu_info.cpu_brand()))
      return false;
  }

  for (size_t i = 0; i < exceptions_.size(); ++i) {
    if (exceptions_[i]->Contains(os_type, os_version, gpu_info) &&
        !exceptions_[i]->NeedsMoreInfo(gpu_info))
      return false;
  }
  return true;
}

bool GpuControlList::GpuControlListEntry::NeedsMoreInfo(
    const GPUInfo& gpu_info) const {
  // We only check for missing info that might be collected with a gl context.
  // If certain info is missing due to some error, say, we fail to collect
  // vendor_id/device_id, then even if we launch GPU process and create a gl
  // context, we won't gather such missing info, so we still return false.
  if (driver_vendor_info_.get() && gpu_info.driver_vendor.empty())
    return true;
  if (driver_version_info_.get() && gpu_info.driver_version.empty())
    return true;
  if (gl_vendor_info_.get() && gpu_info.gl_vendor.empty())
    return true;
  if (gl_renderer_info_.get() && gpu_info.gl_renderer.empty())
    return true;
  for (size_t i = 0; i < exceptions_.size(); ++i) {
    if (exceptions_[i]->NeedsMoreInfo(gpu_info))
      return true;
  }
  return false;
}

GpuControlList::OsType GpuControlList::GpuControlListEntry::GetOsType() const {
  if (os_info_.get() == NULL)
    return kOsAny;
  return os_info_->type();
}

uint32 GpuControlList::GpuControlListEntry::id() const {
  return id_;
}

bool GpuControlList::GpuControlListEntry::disabled() const {
  return disabled_;
}

int GpuControlList::GpuControlListEntry::GetFeatures() const {
  return features_;
}

// static
bool GpuControlList::GpuControlListEntry::StringToFeature(
    const std::string& feature_name, int* feature_id,
    const FeatureMap& feature_map) {
  FeatureMap::const_iterator iter = feature_map.find(feature_name);
  if (iter != feature_map.end()) {
    *feature_id = iter->second;
    return true;
  }
  return false;
}

GpuControlList::GpuControlList()
    : max_entry_id_(0),
      contains_unknown_fields_(false),
      needs_more_info_(false) {
}

GpuControlList::~GpuControlList() {
  Clear();
}

bool GpuControlList::LoadList(
    const std::string& json_context, GpuControlList::OsFilter os_filter) {
  const std::string browser_version_string = "0";
  return LoadList(browser_version_string, json_context, os_filter);
}

bool GpuControlList::LoadList(
    const std::string& browser_version_string,
    const std::string& json_context,
    GpuControlList::OsFilter os_filter) {
  std::vector<std::string> pieces;
  if (!ProcessVersionString(browser_version_string, '.', &pieces))
    return false;
  browser_version_ = browser_version_string;

  scoped_ptr<base::Value> root;
  root.reset(base::JSONReader::Read(json_context));
  if (root.get() == NULL || !root->IsType(base::Value::TYPE_DICTIONARY))
    return false;

  base::DictionaryValue* root_dictionary =
      static_cast<DictionaryValue*>(root.get());
  DCHECK(root_dictionary);
  return LoadList(*root_dictionary, os_filter);
}

bool GpuControlList::LoadList(const base::DictionaryValue& parsed_json,
                              GpuControlList::OsFilter os_filter) {
  std::vector<ScopedGpuControlListEntry> entries;

  parsed_json.GetString("version", &version_);
  std::vector<std::string> pieces;
  if (!ProcessVersionString(version_, '.', &pieces))
    return false;

  const base::ListValue* list = NULL;
  if (!parsed_json.GetList("entries", &list))
    return false;

  uint32 max_entry_id = 0;
  bool contains_unknown_fields = false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* list_item = NULL;
    bool valid = list->GetDictionary(i, &list_item);
    if (!valid || list_item == NULL)
      return false;
    // Check browser version compatibility: if the entry is not for the
    // current browser version, don't process it.
    BrowserVersionSupport browser_version_support =
        IsEntrySupportedByCurrentBrowserVersion(list_item);
    if (browser_version_support == kMalformed)
      return false;
    if (browser_version_support == kUnsupported)
      continue;
    DCHECK(browser_version_support == kSupported);
    ScopedGpuControlListEntry entry(
        GpuControlListEntry::GetEntryFromValue(list_item, true, feature_map_));
    if (entry == NULL)
      return false;
    if (entry->id() > max_entry_id)
      max_entry_id = entry->id();
    // If an unknown field is encountered, skip the entry; if an unknown
    // feature is encountered, ignore the feature, but keep the entry.
    if (entry->contains_unknown_fields()) {
      contains_unknown_fields = true;
      continue;
    }
    if (entry->contains_unknown_features())
      contains_unknown_fields = true;
    entries.push_back(entry);
  }

  Clear();
  OsType my_os = GetOsType();
  for (size_t i = 0; i < entries.size(); ++i) {
    OsType entry_os = entries[i]->GetOsType();
    if (os_filter == GpuControlList::kAllOs ||
        entry_os == kOsAny || entry_os == my_os)
      feature_list_.push_back(entries[i]);
  }
  max_entry_id_ = max_entry_id;
  contains_unknown_fields_ = contains_unknown_fields;
  return true;
}

int GpuControlList::MakeDecision(
    GpuControlList::OsType os,
    std::string os_version,
    const GPUInfo& gpu_info) {
  active_entries_.clear();
  int features = 0;

  needs_more_info_ = false;
  int possible_features = 0;

  if (os == kOsAny)
    os = GetOsType();
  if (os_version.empty()) {
    os_version = base::SysInfo::OperatingSystemVersion();
    size_t pos = os_version.find_first_not_of("0123456789.");
    if (pos != std::string::npos)
      os_version = os_version.substr(0, pos);
  }
  std::vector<std::string> pieces;
  if (!ProcessVersionString(os_version, '.', &pieces))
    os_version = "0";

  for (size_t i = 0; i < feature_list_.size(); ++i) {
    if (feature_list_[i]->Contains(os, os_version, gpu_info)) {
      if (!feature_list_[i]->disabled()) {
        bool not_final = feature_list_[i]->NeedsMoreInfo(gpu_info);
        if (not_final)
          possible_features |= feature_list_[i]->GetFeatures();
        else
          features |= feature_list_[i]->GetFeatures();
      }
      active_entries_.push_back(feature_list_[i]);
    }
  }

  if (possible_features != 0 && (possible_features | features) != features)
    needs_more_info_ = true;

  return features;
}

void GpuControlList::GetDecisionEntries(
    std::vector<uint32>* entry_ids, bool disabled) const {
  DCHECK(entry_ids);
  entry_ids->clear();
  for (size_t i = 0; i < active_entries_.size(); ++i) {
    if (disabled == active_entries_[i]->disabled())
      entry_ids->push_back(active_entries_[i]->id());
  }
}

void GpuControlList::GetReasons(base::ListValue* problem_list) const {
  DCHECK(problem_list);
  for (size_t i = 0; i < active_entries_.size(); ++i) {
    GpuControlListEntry* entry = active_entries_[i];
    if (entry->disabled())
      continue;
    base::DictionaryValue* problem = new base::DictionaryValue();

    problem->SetString("description", entry->description());

    base::ListValue* cr_bugs = new base::ListValue();
    for (size_t j = 0; j < entry->cr_bugs().size(); ++j)
      cr_bugs->Append(new base::FundamentalValue(entry->cr_bugs()[j]));
    problem->Set("crBugs", cr_bugs);

    base::ListValue* webkit_bugs = new base::ListValue();
    for (size_t j = 0; j < entry->webkit_bugs().size(); ++j) {
      webkit_bugs->Append(new base::FundamentalValue(entry->webkit_bugs()[j]));
    }
    problem->Set("webkitBugs", webkit_bugs);

    problem_list->Append(problem);
  }
}

size_t GpuControlList::num_entries() const {
  return feature_list_.size();
}

uint32 GpuControlList::max_entry_id() const {
  return max_entry_id_;
}

std::string GpuControlList::GetVersion() const {
  return version_;
}

GpuControlList::OsType GpuControlList::GetOsType() {
#if defined(OS_CHROMEOS)
  return kOsChromeOS;
#elif defined(OS_WIN)
  return kOsWin;
#elif defined(OS_ANDROID)
  return kOsAndroid;
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  return kOsLinux;
#elif defined(OS_MACOSX)
  return kOsMacosx;
#else
  return kOsUnknown;
#endif
}

void GpuControlList::Clear() {
  feature_list_.clear();
  active_entries_.clear();
  max_entry_id_ = 0;
  contains_unknown_fields_ = false;
}

GpuControlList::BrowserVersionSupport
GpuControlList::IsEntrySupportedByCurrentBrowserVersion(
    const base::DictionaryValue* value) {
  DCHECK(value);
  const base::DictionaryValue* browser_version_value = NULL;
  if (value->GetDictionary("browser_version", &browser_version_value)) {
    std::string version_op = "any";
    std::string version_string;
    std::string version_string2;
    browser_version_value->GetString(kOp, &version_op);
    browser_version_value->GetString("number", &version_string);
    browser_version_value->GetString("number2", &version_string2);
    scoped_ptr<VersionInfo> browser_version_info;
    browser_version_info.reset(
        new VersionInfo(version_op, "", version_string, version_string2));
    if (!browser_version_info->IsValid())
      return kMalformed;
    if (browser_version_info->Contains(browser_version_))
      return kSupported;
    return kUnsupported;
  }
  return kSupported;
}

// static
GpuControlList::NumericOp GpuControlList::StringToNumericOp(
    const std::string& op) {
  if (op == "=")
    return kEQ;
  if (op == "<")
    return kLT;
  if (op == "<=")
    return kLE;
  if (op == ">")
    return kGT;
  if (op == ">=")
    return kGE;
  if (op == "any")
    return kAny;
  if (op == "between")
    return kBetween;
  return kUnknown;
}

void GpuControlList::AddFeature(
    const std::string& feature_name, int feature_id) {
  feature_map_[feature_name] = feature_id;
}

}  // namespace content

