// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_blacklist.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/common/gpu_info.h"

namespace {

// Encode a date as Version, where [0] is year, [1] is month, and [2] is day.
Version* GetDateFromString(const std::string& date_string) {
  // TODO(zmo): verify if in Windows registry, driver dates are always in the
  // format of "mm-dd-yyyy".
  std::vector<std::string> pieces;
  base::SplitString(date_string, '-', &pieces);
  if (pieces.size() != 3)
    return NULL;
  std::string date_as_version_string = pieces[2];
  for (size_t i = 0; i < 2; ++i) {
    date_as_version_string += ".";
    date_as_version_string += pieces[i];
  }
  return Version::GetVersionFromString(date_as_version_string);
}

}  // namespace anonymous

GpuBlacklist::VersionInfo::VersionInfo(const std::string& version_op,
                                       const std::string& version_string,
                                       const std::string& version_string2) {
  op_ = StringToOp(version_op);
  if (op_ == kUnknown || op_ == kAny)
    return;
  version_.reset(Version::GetVersionFromString(version_string));
  if (version_.get() == NULL) {
    op_ = kUnknown;
    return;
  }
  if (op_ == kBetween) {
    version2_.reset(Version::GetVersionFromString(version_string2));
    if (version2_.get() == NULL)
      op_ = kUnknown;
  }
}

GpuBlacklist::VersionInfo::~VersionInfo() {
}

bool GpuBlacklist::VersionInfo::Contains(const Version& version) const {
  if (op_ == kUnknown)
    return false;
  if (op_ == kAny)
    return true;
  if (op_ == kEQ) {
    // Handles cases where 10.6 is considered as containing 10.6.*.
    const std::vector<uint16>& components_reference = version_->components();
    const std::vector<uint16>& components = version.components();
    for (size_t i = 0; i < components_reference.size(); ++i) {
      if (i >= components.size() && components_reference[i] != 0)
        return false;
      if (components[i] != components_reference[i])
        return false;
    }
    return true;
  }
  int relation = version.CompareTo(*version_);
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
  return version.CompareTo(*version2_) <= 0;
}

bool GpuBlacklist::VersionInfo::IsValid() const {
  return op_ != kUnknown;
}

GpuBlacklist::VersionInfo::Op GpuBlacklist::VersionInfo::StringToOp(
    const std::string& version_op) {
  if (version_op == "=")
    return kEQ;
  else if (version_op == "<")
    return kLT;
  else if (version_op == "<=")
    return kLE;
  else if (version_op == ">")
    return kGT;
  else if (version_op == ">=")
    return kGE;
  else if (version_op == "any")
    return kAny;
  else if (version_op == "between")
    return kBetween;
  return kUnknown;
}

GpuBlacklist::OsInfo::OsInfo(const std::string& os,
                             const std::string& version_op,
                             const std::string& version_string,
                             const std::string& version_string2) {
  type_ = StringToOsType(os);
  if (type_ != kOsUnknown) {
    version_info_.reset(
        new VersionInfo(version_op, version_string, version_string2));
  }
}

GpuBlacklist::OsInfo::~OsInfo() {}

bool GpuBlacklist::OsInfo::Contains(OsType type,
                                    const Version& version) const {
  if (!IsValid())
    return false;
  if (type_ != type && type_ != kOsAny)
    return false;
  return version_info_->Contains(version);
}

bool GpuBlacklist::OsInfo::IsValid() const {
  return type_ != kOsUnknown && version_info_->IsValid();
}

GpuBlacklist::OsType GpuBlacklist::OsInfo::type() const {
  return type_;
}

GpuBlacklist::OsType GpuBlacklist::OsInfo::StringToOsType(
    const std::string& os) {
  if (os == "win")
    return kOsWin;
  else if (os == "macosx")
    return kOsMacosx;
  else if (os == "linux")
    return kOsLinux;
  else if (os == "chromeos")
    return kOsChromeOS;
  else if (os == "any")
    return kOsAny;
  return kOsUnknown;
}

GpuBlacklist::StringInfo::StringInfo(const std::string& string_op,
                                     const std::string& string_value) {
  op_ = StringToOp(string_op);
  value_ = StringToLowerASCII(string_value);
}

bool GpuBlacklist::StringInfo::Contains(const std::string& value) const {
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

bool GpuBlacklist::StringInfo::IsValid() const {
  return op_ != kUnknown;
}

GpuBlacklist::StringInfo::Op GpuBlacklist::StringInfo::StringToOp(
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

// static
GpuBlacklist::ScopedGpuBlacklistEntry
GpuBlacklist::GpuBlacklistEntry::GetGpuBlacklistEntryFromValue(
    DictionaryValue* value, bool top_level) {
  DCHECK(value);
  ScopedGpuBlacklistEntry entry(new GpuBlacklistEntry());

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

  ListValue* cr_bugs;
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

  ListValue* webkit_bugs;
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

  DictionaryValue* os_value = NULL;
  if (value->GetDictionary("os", &os_value)) {
    std::string os_type;
    std::string os_version_op = "any";
    std::string os_version_string;
    std::string os_version_string2;
    os_value->GetString("type", &os_type);
    DictionaryValue* os_version_value = NULL;
    if (os_value->GetDictionary("version", &os_version_value)) {
      os_version_value->GetString("op", &os_version_op);
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

  ListValue* device_id_list;
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

  DictionaryValue* driver_vendor_value = NULL;
  if (value->GetDictionary("driver_vendor", &driver_vendor_value)) {
    std::string vendor_op;
    std::string vendor_value;
    driver_vendor_value->GetString("op", &vendor_op);
    driver_vendor_value->GetString("value", &vendor_value);
    if (!entry->SetDriverVendorInfo(vendor_op, vendor_value)) {
      LOG(WARNING) << "Malformed driver_vendor entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  DictionaryValue* driver_version_value = NULL;
  if (value->GetDictionary("driver_version", &driver_version_value)) {
    std::string driver_version_op = "any";
    std::string driver_version_string;
    std::string driver_version_string2;
    driver_version_value->GetString("op", &driver_version_op);
    driver_version_value->GetString("number", &driver_version_string);
    driver_version_value->GetString("number2", &driver_version_string2);
    if (!entry->SetDriverVersionInfo(driver_version_op, driver_version_string,
                                     driver_version_string2)) {
      LOG(WARNING) << "Malformed driver_version entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  DictionaryValue* driver_date_value = NULL;
  if (value->GetDictionary("driver_date", &driver_date_value)) {
    std::string driver_date_op = "any";
    std::string driver_date_string;
    std::string driver_date_string2;
    driver_date_value->GetString("op", &driver_date_op);
    driver_date_value->GetString("number", &driver_date_string);
    driver_date_value->GetString("number2", &driver_date_string2);
    if (!entry->SetDriverDateInfo(driver_date_op, driver_date_string,
                                  driver_date_string2)) {
      LOG(WARNING) << "Malformed driver_date entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  DictionaryValue* gl_vendor_value = NULL;
  if (value->GetDictionary("gl_vendor", &gl_vendor_value)) {
    std::string vendor_op;
    std::string vendor_value;
    gl_vendor_value->GetString("op", &vendor_op);
    gl_vendor_value->GetString("value", &vendor_value);
    if (!entry->SetGLVendorInfo(vendor_op, vendor_value)) {
      LOG(WARNING) << "Malformed gl_vendor entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  DictionaryValue* gl_renderer_value = NULL;
  if (value->GetDictionary("gl_renderer", &gl_renderer_value)) {
    std::string renderer_op;
    std::string renderer_value;
    gl_renderer_value->GetString("op", &renderer_op);
    gl_renderer_value->GetString("value", &renderer_value);
    if (!entry->SetGLRendererInfo(renderer_op, renderer_value)) {
      LOG(WARNING) << "Malformed gl_renderer entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  if (top_level) {
    ListValue* blacklist_value = NULL;
    if (!value->GetList("blacklist", &blacklist_value)) {
      LOG(WARNING) << "Malformed blacklist entry " << entry->id();
      return NULL;
    }
    std::vector<std::string> blacklist;
    for (size_t i = 0; i < blacklist_value->GetSize(); ++i) {
      std::string feature;
      if (blacklist_value->GetString(i, &feature)) {
        blacklist.push_back(feature);
      } else {
        LOG(WARNING) << "Malformed blacklist entry " << entry->id();
        return NULL;
      }
    }
    if (!entry->SetBlacklistedFeatures(blacklist)) {
      LOG(WARNING) << "Malformed blacklist entry " << entry->id();
      return NULL;
    }
    dictionary_entry_count++;
  }

  if (top_level) {
    ListValue* exception_list_value = NULL;
    if (value->GetList("exceptions", &exception_list_value)) {
      for (size_t i = 0; i < exception_list_value->GetSize(); ++i) {
        DictionaryValue* exception_value = NULL;
        if (!exception_list_value->GetDictionary(i, &exception_value)) {
          LOG(WARNING) << "Malformed exceptions entry " << entry->id();
          return NULL;
        }
        ScopedGpuBlacklistEntry exception(
            GetGpuBlacklistEntryFromValue(exception_value, false));
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

    DictionaryValue* browser_version_value = NULL;
    // browser_version is processed in LoadGpuBlacklist().
    if (value->GetDictionary("browser_version", &browser_version_value))
      dictionary_entry_count++;
  }

  if (value->size() != dictionary_entry_count) {
    LOG(WARNING) << "Entry with unknown fields " << entry->id();
    entry->contains_unknown_fields_ = true;
  }
  return entry;
}

GpuBlacklist::GpuBlacklistEntry::GpuBlacklistEntry()
    : id_(0),
      disabled_(false),
      vendor_id_(0),
      contains_unknown_fields_(false),
      contains_unknown_features_(false) {
}

bool GpuBlacklist::GpuBlacklistEntry::SetId(uint32 id) {
  if (id != 0) {
    id_ = id;
    return true;
  }
  return false;
}

void GpuBlacklist::GpuBlacklistEntry::SetDisabled(bool disabled) {
  disabled_ = disabled;
}

bool GpuBlacklist::GpuBlacklistEntry::SetOsInfo(
    const std::string& os,
    const std::string& version_op,
    const std::string& version_string,
    const std::string& version_string2) {
  os_info_.reset(new OsInfo(os, version_op, version_string, version_string2));
  return os_info_->IsValid();
}

bool GpuBlacklist::GpuBlacklistEntry::SetVendorId(
    const std::string& vendor_id_string) {
  vendor_id_ = 0;
  return base::HexStringToInt(vendor_id_string,
                              reinterpret_cast<int*>(&vendor_id_));
}

bool GpuBlacklist::GpuBlacklistEntry::AddDeviceId(
    const std::string& device_id_string) {
  uint32 device_id = 0;
  if (base::HexStringToInt(device_id_string,
                           reinterpret_cast<int*>(&device_id))) {
    device_id_list_.push_back(device_id);
    return true;
  }
  return false;
}

bool GpuBlacklist::GpuBlacklistEntry::SetDriverVendorInfo(
    const std::string& vendor_op,
    const std::string& vendor_value) {
  driver_vendor_info_.reset(
      new StringInfo(vendor_op, vendor_value));
  return driver_vendor_info_->IsValid();
}

bool GpuBlacklist::GpuBlacklistEntry::SetDriverVersionInfo(
    const std::string& version_op,
    const std::string& version_string,
    const std::string& version_string2) {
  driver_version_info_.reset(
      new VersionInfo(version_op, version_string, version_string2));
  return driver_version_info_->IsValid();
}

bool GpuBlacklist::GpuBlacklistEntry::SetDriverDateInfo(
    const std::string& date_op,
    const std::string& date_string,
    const std::string& date_string2) {
  driver_date_info_.reset(
      new VersionInfo(date_op, date_string, date_string2));
  return driver_date_info_->IsValid();
}

bool GpuBlacklist::GpuBlacklistEntry::SetGLVendorInfo(
    const std::string& vendor_op,
    const std::string& vendor_value) {
  gl_vendor_info_.reset(
      new StringInfo(vendor_op, vendor_value));
  return gl_vendor_info_->IsValid();
}

bool GpuBlacklist::GpuBlacklistEntry::SetGLRendererInfo(
    const std::string& renderer_op,
    const std::string& renderer_value) {
  gl_renderer_info_.reset(
      new StringInfo(renderer_op, renderer_value));
  return gl_renderer_info_->IsValid();
}

bool GpuBlacklist::GpuBlacklistEntry::SetBlacklistedFeatures(
    const std::vector<std::string>& blacklisted_features) {
  size_t size = blacklisted_features.size();
  if (size == 0)
    return false;
  uint32 flags = 0;
  for (size_t i = 0; i < size; ++i) {
    GpuFeatureFlags::GpuFeatureType type =
        GpuFeatureFlags::StringToGpuFeatureType(blacklisted_features[i]);
    switch (type) {
      case GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas:
      case GpuFeatureFlags::kGpuFeatureAcceleratedCompositing:
      case GpuFeatureFlags::kGpuFeatureWebgl:
      case GpuFeatureFlags::kGpuFeatureMultisampling:
      case GpuFeatureFlags::kGpuFeatureAll:
        flags |= type;
        break;
      case GpuFeatureFlags::kGpuFeatureUnknown:
        contains_unknown_features_ = true;
        break;
    }
  }
  feature_flags_.reset(new GpuFeatureFlags());
  feature_flags_->set_flags(flags);
  return true;
}

void GpuBlacklist::GpuBlacklistEntry::AddException(
    ScopedGpuBlacklistEntry exception) {
  exceptions_.push_back(exception);
}

bool GpuBlacklist::GpuBlacklistEntry::Contains(
    OsType os_type, const Version& os_version,
    const content::GPUInfo& gpu_info) const {
  DCHECK(os_type != kOsAny);
  if (os_info_.get() != NULL && !os_info_->Contains(os_type, os_version))
    return false;
  if (vendor_id_ != 0 && vendor_id_ != gpu_info.vendor_id)
    return false;
  if (device_id_list_.size() > 0) {
    bool found = false;
    for (size_t i = 0; i < device_id_list_.size(); ++i) {
      if (device_id_list_[i] == gpu_info.device_id) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }
  if (driver_vendor_info_.get() != NULL &&
      !driver_vendor_info_->Contains(gpu_info.driver_vendor))
    return false;
  if (driver_version_info_.get() != NULL) {
    scoped_ptr<Version> driver_version(
        Version::GetVersionFromString(gpu_info.driver_version));
    if (driver_version.get() == NULL ||
        !driver_version_info_->Contains(*driver_version))
      return false;
  }
  if (driver_date_info_.get() != NULL) {
    scoped_ptr<Version> driver_date(GetDateFromString(gpu_info.driver_date));
    if (driver_date.get() == NULL ||
        !driver_date_info_->Contains(*driver_date))
      return false;
  }
  if (gl_vendor_info_.get() != NULL &&
      !gl_vendor_info_->Contains(gpu_info.gl_vendor))
    return false;
  if (gl_renderer_info_.get() != NULL &&
      !gl_renderer_info_->Contains(gpu_info.gl_renderer))
    return false;
  for (size_t i = 0; i < exceptions_.size(); ++i) {
    if (exceptions_[i]->Contains(os_type, os_version, gpu_info))
      return false;
  }
  return true;
}

GpuBlacklist::OsType GpuBlacklist::GpuBlacklistEntry::GetOsType() const {
  if (os_info_.get() == NULL)
    return kOsAny;
  return os_info_->type();
}

uint32 GpuBlacklist::GpuBlacklistEntry::id() const {
  return id_;
}

bool GpuBlacklist::GpuBlacklistEntry::disabled() const {
  return disabled_;
}

GpuFeatureFlags GpuBlacklist::GpuBlacklistEntry::GetGpuFeatureFlags() const {
  return *feature_flags_;
}

GpuBlacklist::GpuBlacklist(const std::string& browser_version_string)
    : max_entry_id_(0),
      contains_unknown_fields_(false) {
  SetBrowserVersion(browser_version_string);
}

GpuBlacklist::~GpuBlacklist() {
  Clear();
}

bool GpuBlacklist::LoadGpuBlacklist(
    const std::string& json_context, GpuBlacklist::OsFilter os_filter) {
  scoped_ptr<Value> root;
  root.reset(base::JSONReader::Read(json_context, false));
  if (root.get() == NULL || !root->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* root_dictionary = static_cast<DictionaryValue*>(root.get());
  DCHECK(root_dictionary);
  return LoadGpuBlacklist(*root_dictionary, os_filter);
}

bool GpuBlacklist::LoadGpuBlacklist(
    const DictionaryValue& parsed_json, GpuBlacklist::OsFilter os_filter) {
  std::vector<ScopedGpuBlacklistEntry> entries;

  std::string version_string;
  parsed_json.GetString("version", &version_string);
  version_.reset(Version::GetVersionFromString(version_string));
  if (version_.get() == NULL)
    return false;

  ListValue* list = NULL;
  if (!parsed_json.GetList("entries", &list))
    return false;

  uint32 max_entry_id = 0;
  bool contains_unknown_fields = false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    DictionaryValue* list_item = NULL;
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
    ScopedGpuBlacklistEntry entry(
        GpuBlacklistEntry::GetGpuBlacklistEntryFromValue(list_item, true));
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
    if (os_filter == GpuBlacklist::kAllOs ||
        entry_os == kOsAny || entry_os == my_os)
      blacklist_.push_back(entries[i]);
  }
  max_entry_id_ = max_entry_id;
  contains_unknown_fields_ = contains_unknown_fields;
  return true;
}

GpuFeatureFlags GpuBlacklist::DetermineGpuFeatureFlags(
    GpuBlacklist::OsType os,
    Version* os_version,
    const content::GPUInfo& gpu_info) {
  active_entries_.clear();
  GpuFeatureFlags flags;

  if (os == kOsAny)
    os = GetOsType();
  scoped_ptr<Version> my_os_version;
  if (os_version == NULL) {
    std::string version_string = base::SysInfo::OperatingSystemVersion();
    size_t pos = version_string.find_first_not_of("0123456789.");
    if (pos != std::string::npos)
      version_string = version_string.substr(0, pos);
    my_os_version.reset(Version::GetVersionFromString(version_string));
    os_version = my_os_version.get();
  }
  DCHECK(os_version != NULL);

  for (size_t i = 0; i < blacklist_.size(); ++i) {
    if (blacklist_[i]->Contains(os, *os_version, gpu_info)) {
      if (!blacklist_[i]->disabled())
        flags.Combine(blacklist_[i]->GetGpuFeatureFlags());
      active_entries_.push_back(blacklist_[i]);
    }
  }
  return flags;
}

void GpuBlacklist::GetGpuFeatureFlagEntries(
    GpuFeatureFlags::GpuFeatureType feature,
    std::vector<uint32>& entry_ids,
    bool disabled) const {
  entry_ids.clear();
  for (size_t i = 0; i < active_entries_.size(); ++i) {
    if (((feature & active_entries_[i]->GetGpuFeatureFlags().flags()) != 0) &&
        disabled == active_entries_[i]->disabled())
      entry_ids.push_back(active_entries_[i]->id());
  }
}

void GpuBlacklist::GetBlacklistReasons(ListValue* problem_list) const {
  DCHECK(problem_list);
  for (size_t i = 0; i < active_entries_.size(); ++i) {
    GpuBlacklistEntry* entry = active_entries_[i];
    if (entry->disabled())
      continue;
    DictionaryValue* problem = new DictionaryValue();

    problem->SetString("description", entry->description());

    ListValue* cr_bugs = new ListValue();
    for (size_t j = 0; j < entry->cr_bugs().size(); ++j)
      cr_bugs->Append(Value::CreateIntegerValue(entry->cr_bugs()[j]));
    problem->Set("crBugs", cr_bugs);

    ListValue* webkit_bugs = new ListValue();
    for (size_t j = 0; j < entry->webkit_bugs().size(); ++j) {
      webkit_bugs->Append(Value::CreateIntegerValue(
          entry->webkit_bugs()[j]));
    }
    problem->Set("webkitBugs", webkit_bugs);

    problem_list->Append(problem);
  }
}

size_t GpuBlacklist::num_entries() const {
  return blacklist_.size();
}

uint32 GpuBlacklist::max_entry_id() const {
  return max_entry_id_;
}

bool GpuBlacklist::GetVersion(uint16* major, uint16* minor) const {
  DCHECK(major && minor);
  *major = 0;
  *minor = 0;
  if (version_.get() == NULL)
    return false;
  const std::vector<uint16>& components_reference = version_->components();
  if (components_reference.size() != 2)
    return false;
  *major = components_reference[0];
  *minor = components_reference[1];
  return true;
}

bool GpuBlacklist::GetVersion(
    const DictionaryValue& parsed_json, uint16* major, uint16* minor) {
  DCHECK(major && minor);
  *major = 0;
  *minor = 0;
  std::string version_string;
  if (!parsed_json.GetString("version", &version_string))
    return false;
  scoped_ptr<Version> version(Version::GetVersionFromString(version_string));
  if (version.get() == NULL)
    return false;
  const std::vector<uint16>& components_reference = version->components();
  if (components_reference.size() != 2)
    return false;
  *major = components_reference[0];
  *minor = components_reference[1];
  return true;
}

GpuBlacklist::OsType GpuBlacklist::GetOsType() {
#if defined(OS_CHROMEOS)
  return kOsChromeOS;
#elif defined(OS_WIN)
  return kOsWin;
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  return kOsLinux;
#elif defined(OS_MACOSX)
  return kOsMacosx;
#else
  return kOsUnknown;
#endif
}

void GpuBlacklist::Clear() {
  blacklist_.clear();
  active_entries_.clear();
  max_entry_id_ = 0;
  contains_unknown_fields_ = false;
}

GpuBlacklist::BrowserVersionSupport
GpuBlacklist::IsEntrySupportedByCurrentBrowserVersion(
    DictionaryValue* value) {
  DCHECK(value);
  DictionaryValue* browser_version_value = NULL;
  if (value->GetDictionary("browser_version", &browser_version_value)) {
    std::string version_op = "any";
    std::string version_string;
    std::string version_string2;
    browser_version_value->GetString("op", &version_op);
    browser_version_value->GetString("number", &version_string);
    browser_version_value->GetString("number2", &version_string2);
    scoped_ptr<VersionInfo> browser_version_info;
    browser_version_info.reset(
      new VersionInfo(version_op, version_string, version_string2));
    if (!browser_version_info->IsValid())
      return kMalformed;
    if (browser_version_info->Contains(*browser_version_))
      return kSupported;
    return kUnsupported;
  }
  return kSupported;
}

void GpuBlacklist::SetBrowserVersion(const std::string& version_string) {
  browser_version_.reset(Version::GetVersionFromString(version_string));
  DCHECK(browser_version_.get() != NULL);
}

