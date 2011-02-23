// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BLACKLIST_H_
#define CONTENT_BROWSER_GPU_BLACKLIST_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/gpu_feature_flags.h"

class DictionaryValue;
class GPUInfo;
class Version;

class GpuBlacklist {
 public:
  enum OsType {
    kOsLinux,
    kOsMacosx,
    kOsWin,
    kOsAny,
    kOsUnknown
  };

  GpuBlacklist();
  ~GpuBlacklist();

  // Loads blacklist information from a json file.
  // current_os_only==true indicates all blacklist entries that don't belong to
  // the current OS are discarded; current_os_only==false should only be used
  // for testing purpose.
  // If failed, the current GpuBlacklist is un-touched.
  bool LoadGpuBlacklist(const std::string& json_context,
                        bool current_os_only);

  // Collects system information and combines them with gpu_info and blacklist
  // information to determine gpu feature flags.
  // If os is kOsAny, use the current OS; if os_version is null, use the
  // current OS version.
  GpuFeatureFlags DetermineGpuFeatureFlags(OsType os,
                                           Version* os_version,
                                           const GPUInfo& gpu_info);

  // Collects the entries that set the "feature" flag from the last
  // DetermineGpuFeatureFlags() call.  This tells which entries are responsible
  // for raising a certain flag, i.e, for blacklisting a certain feature.
  // Examples of "feature":
  //   kGpuFeatureAll - any of the supported features;
  //   kGpuFeatureWebgl - a single feature;
  //   kGpuFeatureWebgl | kGpuFeatureAcceleratedCompositing - two features.
  void GetGpuFeatureFlagEntries(GpuFeatureFlags::GpuFeatureType feature,
                                std::vector<uint32>& entry_ids) const;

  // Return the largest entry id.  This is used for histogramming.
  uint32 max_entry_id() const;

  // Collects the version of the current blacklist.  Returns false and sets
  // major and minor to 0 on failure.
  bool GetVersion(uint16* major, uint16* monir) const;

 private:
  class VersionInfo {
   public:
    VersionInfo(const std::string& version_op,
                const std::string& version_string,
                const std::string& version_string2);
    ~VersionInfo();

    // Determines if a given version is included in the VersionInfo range.
    bool Contains(const Version& version) const;

    // Determines if the VersionInfo contains valid information.
    bool IsValid() const;

   private:
    enum Op {
      kBetween,  // <= * <=
      kEQ,  // =
      kLT,  // <
      kLE,  // <=
      kGT,  // >
      kGE,  // >=
      kAny,
      kUnknown  // Indicates VersionInfo data is invalid.
    };

    // Maps string to Op; returns kUnknown if it's not a valid Op.
    static Op StringToOp(const std::string& version_op);

    Op op_;
    scoped_ptr<Version> version_;
    scoped_ptr<Version> version2_;
  };

  class OsInfo {
   public:
    OsInfo(const std::string& os,
           const std::string& version_op,
           const std::string& version_string,
           const std::string& version_string2);
    ~OsInfo();

    // Determines if a given os/version is included in the OsInfo set.
    bool Contains(OsType type, const Version& version) const;

    // Determines if the VersionInfo contains valid information.
    bool IsValid() const;

    OsType type() const;

    // Maps string to OsType; returns kOsUnknown if it's not a valid os.
    static OsType StringToOsType(const std::string& os);

   private:
    OsType type_;
    scoped_ptr<VersionInfo> version_info_;
  };

  class StringInfo {
   public:
    StringInfo(const std::string& string_op, const std::string& string_value);

    // Determines if a given string is included in the StringInfo.
    bool Contains(const std::string& value) const;

    // Determines if the StringInfo contains valid information.
    bool IsValid() const;

   private:
    enum Op {
      kContains,
      kBeginWith,
      kEndWith,
      kEQ,  // =
      kUnknown  // Indicates StringInfo data is invalid.
    };

    // Maps string to Op; returns kUnknown if it's not a valid Op.
    static Op StringToOp(const std::string& string_op);

    Op op_;
    std::string value_;
  };

  class GpuBlacklistEntry {
   public:
    // Constructs GpuBlacklistEntry from DictionaryValue loaded from json.
    static GpuBlacklistEntry* GetGpuBlacklistEntryFromValue(
        DictionaryValue* value);

    // Determines if a given os/gc/driver is included in the Entry set.
    bool Contains(OsType os_type,
                  const Version& os_version,
                  const GPUInfo& gpu_info) const;

    // Returns the OsType.
    OsType GetOsType() const;

    // Returns the entry's unique id.  0 is reserved.
    uint32 id() const;

    // Returns the GpuFeatureFlags.
    GpuFeatureFlags GetGpuFeatureFlags() const;

    ~GpuBlacklistEntry();

   private:
    GpuBlacklistEntry();

    bool SetId(const std::string& id_string);

    bool SetOsInfo(const std::string& os,
                   const std::string& version_op,
                   const std::string& version_string,
                   const std::string& version_string2);

    bool SetVendorId(const std::string& vendor_id_string);

    bool SetDeviceId(const std::string& device_id_string);

    bool SetDriverVendorInfo(const std::string& vendor_op,
                             const std::string& vendor_value);

    bool SetDriverVersionInfo(const std::string& version_op,
                              const std::string& version_string,
                              const std::string& version_string2);

    bool SetGLRendererInfo(const std::string& renderer_op,
                           const std::string& renderer_value);

    bool SetBlacklistedFeatures(
        const std::vector<std::string>& blacklisted_features);

    uint32 id_;
    scoped_ptr<OsInfo> os_info_;
    uint32 vendor_id_;
    uint32 device_id_;
    scoped_ptr<StringInfo> driver_vendor_info_;
    scoped_ptr<VersionInfo> driver_version_info_;
    scoped_ptr<StringInfo> gl_renderer_info_;
    scoped_ptr<GpuFeatureFlags> feature_flags_;
  };

  // Gets the current OS type.
  static OsType GetOsType();

  void Clear();

  scoped_ptr<Version> version_;
  std::vector<GpuBlacklistEntry*> blacklist_;

  // This records all the blacklist entries that are appliable to the current
  // user machine.  It is updated everytime DetermineGpuFeatureFlags() is
  // called and is used later by GetGpuFeatureFlagEntries().
  std::vector<GpuBlacklistEntry*> active_entries_;

  uint32 max_entry_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuBlacklist);
};

#endif  // CONTENT_BROWSER_GPU_BLACKLIST_H_
