// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_blacklist.h"

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

GpuBlacklist::GpuBlacklistEntry*
GpuBlacklist::GpuBlacklistEntry::GetGpuBlacklistEntryFromValue(
    DictionaryValue* value) {
  if (value == NULL)
    return NULL;

  GpuBlacklistEntry* entry = new GpuBlacklistEntry();

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

  return entry;
}

GpuBlacklist::GpuBlacklistEntry::~GpuBlacklistEntry() {}

GpuBlacklist::GpuBlacklistEntry::GpuBlacklistEntry()
    : vendor_id_(0),
      device_id_(0) {
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

bool GpuBlacklist::GpuBlacklistEntry::SetDriverVersionInfo(
    const std::string& version_op,
    const std::string& version_string,
    const std::string& version_string2) {
  driver_version_info_.reset(
      new VersionInfo(version_op, version_string, version_string2));
  return driver_version_info_->IsValid();
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

bool GpuBlacklist::GpuBlacklistEntry::Contains(
    OsType os_type, const Version& os_version,
    uint32 vendor_id, uint32 device_id,
    const Version& driver_version) const {
  DCHECK(os_type != kOsAny);
  if (os_info_.get() != NULL && !os_info_->Contains(os_type, os_version))
    return false;
  if (vendor_id_ != 0 && vendor_id_ != vendor_id)
    return false;
  if (device_id_ != 0 && device_id_ != device_id)
    return false;
  if (driver_version_info_.get() == NULL)
    return true;
  return driver_version_info_->Contains(driver_version);
}

GpuBlacklist::OsType GpuBlacklist::GpuBlacklistEntry::GetOsType() const {
  if (os_info_.get() == NULL)
    return kOsUnknown;
  return os_info_->type();
}

GpuFeatureFlags GpuBlacklist::GpuBlacklistEntry::GetGpuFeatureFlags() const {
  return *feature_flags_;
}

GpuBlacklist::GpuBlacklist() {
}

GpuBlacklist::~GpuBlacklist() {
  Clear();
}

bool GpuBlacklist::LoadGpuBlacklist(const std::string& json_context,
                                    bool current_os_only) {
  std::vector<GpuBlacklistEntry*> entries;
  scoped_ptr<Value> root;
  root.reset(base::JSONReader::Read(json_context, false));
  if (root.get() == NULL || !root->IsType(Value::TYPE_DICTIONARY))
    return false;

  ListValue* list = NULL;
  static_cast<DictionaryValue*>(root.get())->GetList("entries", &list);
  if (list == NULL)
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    DictionaryValue* list_item = NULL;
    bool valid = list->GetDictionary(i, &list_item);
    if (!valid)
      break;
    GpuBlacklistEntry* entry =
        GpuBlacklistEntry::GetGpuBlacklistEntryFromValue(list_item);
    if (entry == NULL)
      break;
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
  return true;
}

GpuFeatureFlags GpuBlacklist::DetermineGpuFeatureFlags(
    GpuBlacklist::OsType os,
    Version* os_version,
    const GPUInfo& gpu_info) const {
  GpuFeatureFlags flags;
  // No need to go through blacklist entries if GPUInfo isn't available.
  if (gpu_info.progress() == GPUInfo::kUninitialized)
    return flags;
  scoped_ptr<Version> driver_version(
      Version::GetVersionFromString(gpu_info.driver_version()));
  if (driver_version.get() == NULL)
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
    if (blacklist_[i]->Contains(os, *os_version,
                                gpu_info.vendor_id(), gpu_info.device_id(),
                                *driver_version)) {
      flags.Combine(blacklist_[i]->GetGpuFeatureFlags());
    }
  }
  return flags;
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
}

