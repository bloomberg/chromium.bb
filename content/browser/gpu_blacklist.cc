// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu_blacklist.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/gpu_info.h"

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

GpuBlacklist::GpuBlacklistEntry*
GpuBlacklist::GpuBlacklistEntry::GetGpuBlacklistEntryFromValue(
    DictionaryValue* value, bool top_level) {
  if (value == NULL)
    return NULL;

  GpuBlacklistEntry* entry = new GpuBlacklistEntry();

  if (top_level) {
    std::string id;
    if (!value->GetString("id", &id) || !entry->SetId(id)) {
      delete entry;
      return NULL;
    }
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
      delete entry;
      return NULL;
    }
  }

  std::string vendor_id;
  if (value->GetString("vendor_id", &vendor_id)) {
    if (!entry->SetVendorId(vendor_id)) {
      delete entry;
      return NULL;
    }
  }

  std::string device_id;
  if (value->GetString("device_id", &device_id)) {
    if (!entry->SetDeviceId(device_id)) {
      delete entry;
      return NULL;
    }
  }

  DictionaryValue* driver_vendor_value = NULL;
  if (value->GetDictionary("driver_vendor", &driver_vendor_value)) {
    std::string vendor_op;
    std::string vendor_value;
    driver_vendor_value->GetString("op", &vendor_op);
    driver_vendor_value->GetString("value", &vendor_value);
    if (!entry->SetDriverVendorInfo(vendor_op, vendor_value)) {
      delete entry;
      return NULL;
    }
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
      delete entry;
      return NULL;
    }
  }

  DictionaryValue* gl_renderer_value = NULL;
  if (value->GetDictionary("gl_renderer", &gl_renderer_value)) {
    std::string renderer_op;
    std::string renderer_value;
    gl_renderer_value->GetString("op", &renderer_op);
    gl_renderer_value->GetString("value", &renderer_value);
    if (!entry->SetGLRendererInfo(renderer_op, renderer_value)) {
      delete entry;
      return NULL;
    }
  }

  if (top_level) {
    ListValue* blacklist_value = NULL;
    if (!value->GetList("blacklist", &blacklist_value)) {
      delete entry;
      return NULL;
    }
    std::vector<std::string> blacklist;
    for (size_t i = 0; i < blacklist_value->GetSize(); ++i) {
      std::string feature;
      if (blacklist_value->GetString(i, &feature)) {
        blacklist.push_back(feature);
      } else {
        delete entry;
        return NULL;
      }
    }
    if (!entry->SetBlacklistedFeatures(blacklist)) {
      delete entry;
      return NULL;
    }
  }

  if (top_level) {
    ListValue* exception_list_value = NULL;
    if (value->GetList("exceptions", &exception_list_value)) {
      for (size_t i = 0; i < exception_list_value->GetSize(); ++i) {
        DictionaryValue* exception_value = NULL;
        if (!exception_list_value->GetDictionary(i, &exception_value))
          continue;
        GpuBlacklistEntry* exception = GetGpuBlacklistEntryFromValue(
            exception_value, false);
        if (exception)
          entry->AddException(exception);
      }
    }
  }

  return entry;
}

GpuBlacklist::GpuBlacklistEntry::~GpuBlacklistEntry() {
  for (size_t i = 0; i < exceptions_.size(); ++i)
    delete exceptions_[i];
}

GpuBlacklist::GpuBlacklistEntry::GpuBlacklistEntry()
    : id_(0),
      vendor_id_(0),
      device_id_(0) {
}

bool GpuBlacklist::GpuBlacklistEntry::SetId(
    const std::string& id_string) {
  int my_id;
  if (base::HexStringToInt(id_string, &my_id) && my_id != 0) {
    id_ = static_cast<uint32>(my_id);
    return true;
  }
  return false;
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

bool GpuBlacklist::GpuBlacklistEntry::SetDeviceId(
    const std::string& device_id_string) {
  device_id_ = 0;
  return base::HexStringToInt(device_id_string,
                              reinterpret_cast<int*>(&device_id_));
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
      case GpuFeatureFlags::kGpuFeatureAll:
        flags |= type;
        break;
      case GpuFeatureFlags::kGpuFeatureUnknown:
        return false;
    }
  }
  feature_flags_.reset(new GpuFeatureFlags());
  feature_flags_->set_flags(flags);
  return true;
}

void GpuBlacklist::GpuBlacklistEntry::AddException(
    GpuBlacklistEntry* exception) {
  exceptions_.push_back(exception);
}

bool GpuBlacklist::GpuBlacklistEntry::Contains(
    OsType os_type, const Version& os_version, const GPUInfo& gpu_info) const {
  DCHECK(os_type != kOsAny);
  if (os_info_.get() != NULL && !os_info_->Contains(os_type, os_version))
    return false;
  if (vendor_id_ != 0 && vendor_id_ != gpu_info.vendor_id())
    return false;
  if (device_id_ != 0 && device_id_ != gpu_info.device_id())
    return false;
  if (driver_vendor_info_.get() != NULL &&
      !driver_vendor_info_->Contains(gpu_info.driver_vendor()))
    return false;
  if (driver_version_info_.get() != NULL) {
    scoped_ptr<Version> driver_version(
        Version::GetVersionFromString(gpu_info.driver_version()));
    if (driver_version.get() == NULL ||
        !driver_version_info_->Contains(*driver_version))
      return false;
  }
  if (gl_renderer_info_.get() != NULL &&
      !gl_renderer_info_->Contains(gpu_info.gl_renderer()))
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

GpuFeatureFlags GpuBlacklist::GpuBlacklistEntry::GetGpuFeatureFlags() const {
  return *feature_flags_;
}

GpuBlacklist::GpuBlacklist()
    : max_entry_id_(0) {
}

GpuBlacklist::~GpuBlacklist() {
  Clear();
}

bool GpuBlacklist::LoadGpuBlacklist(const std::string& json_context,
                                    bool current_os_only) {
  scoped_ptr<Value> root;
  root.reset(base::JSONReader::Read(json_context, false));
  if (root.get() == NULL || !root->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* root_dictionary = static_cast<DictionaryValue*>(root.get());
  DCHECK(root_dictionary);
  return LoadGpuBlacklist(*root_dictionary, current_os_only);
}

bool GpuBlacklist::LoadGpuBlacklist(const DictionaryValue& parsed_json,
                                    bool current_os_only) {
  std::vector<GpuBlacklistEntry*> entries;

  std::string version_string;
  parsed_json.GetString("version", &version_string);
  version_.reset(Version::GetVersionFromString(version_string));
  if (version_.get() == NULL)
    return false;

  ListValue* list = NULL;
  parsed_json.GetList("entries", &list);
  if (list == NULL)
    return false;

  uint32 max_entry_id = 0;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    DictionaryValue* list_item = NULL;
    bool valid = list->GetDictionary(i, &list_item);
    if (!valid)
      break;
    GpuBlacklistEntry* entry =
        GpuBlacklistEntry::GetGpuBlacklistEntryFromValue(list_item, true);
    if (entry == NULL)
      break;
    if (entry->id() > max_entry_id)
      max_entry_id = entry->id();
    entries.push_back(entry);
  }

  if (entries.size() < list->GetSize()) {
    for (size_t i = 0; i < entries.size(); ++i)
      delete entries[i];
    return false;
  }

  Clear();
  // Don't apply GPU blacklist for a non-registered OS.
  OsType os_filter = GetOsType();
  if (os_filter != kOsUnknown) {
    for (size_t i = 0; i < entries.size(); ++i) {
      OsType entry_os = entries[i]->GetOsType();
      if (!current_os_only ||
          entry_os == kOsAny || entry_os == os_filter)
        blacklist_.push_back(entries[i]);
      else
        delete entries[i];
    }
  }
  max_entry_id_ = max_entry_id;
  return true;
}

GpuFeatureFlags GpuBlacklist::DetermineGpuFeatureFlags(
    GpuBlacklist::OsType os,
    Version* os_version,
    const GPUInfo& gpu_info) {
  active_entries_.clear();
  GpuFeatureFlags flags;
  // No need to go through blacklist entries if GPUInfo isn't available.
  if (gpu_info.level() == GPUInfo::kUninitialized)
    return flags;

  if (os == kOsAny)
    os = GetOsType();
  scoped_ptr<Version> my_os_version;
  if (os_version == NULL) {
    std::string version_string;
#if defined(OS_MACOSX)
    // Seems like base::SysInfo::OperatingSystemVersion() returns the wrong
    // version in MacOsx.
    int32 version_major, version_minor, version_bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(
        &version_major, &version_minor, &version_bugfix);
    version_string = base::StringPrintf("%d.%d.%d",
                                        version_major,
                                        version_minor,
                                        version_bugfix);
#else
    version_string = base::SysInfo::OperatingSystemVersion();
    size_t pos = version_string.find_first_not_of("0123456789.");
    if (pos != std::string::npos)
      version_string = version_string.substr(0, pos);
#endif
    my_os_version.reset(Version::GetVersionFromString(version_string));
    os_version = my_os_version.get();
  }
  DCHECK(os_version != NULL);

  for (size_t i = 0; i < blacklist_.size(); ++i) {
    if (blacklist_[i]->Contains(os, *os_version, gpu_info)) {
      flags.Combine(blacklist_[i]->GetGpuFeatureFlags());
      active_entries_.push_back(blacklist_[i]);
    }
  }
  return flags;
}

void GpuBlacklist::GetGpuFeatureFlagEntries(
    GpuFeatureFlags::GpuFeatureType feature,
    std::vector<uint32>& entry_ids) const {
  entry_ids.clear();
  for (size_t i = 0; i < active_entries_.size(); ++i) {
    if ((feature & active_entries_[i]->GetGpuFeatureFlags().flags()) != 0)
      entry_ids.push_back(active_entries_[i]->id());
  }
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
#if defined(OS_WIN)
  return kOsWin;
#elif defined(OS_LINUX)
  return kOsLinux;
#elif defined(OS_MACOSX)
  return kOsMacosx;
#else
  return kOsUnknown;
#endif
}

void GpuBlacklist::Clear() {
  for (size_t i = 0; i < blacklist_.size(); ++i)
    delete blacklist_[i];
  blacklist_.clear();
  active_entries_.clear();
}

