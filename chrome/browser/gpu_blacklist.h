// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_BLACKLIST_H_
#define CHROME_BROWSER_GPU_BLACKLIST_H_
#pragma once

// Determines whether certain gpu-related features are blacklisted or not.
// A valid gpu_blacklist.json file are in the format of
// {
//   "entries": [
//     { // entry 1
//     },
//     ...
//     { // entry n
//     }
//   ]
// }
// Each entry contains the following fields:
// "os", "vendor_id", "device_id", "driver_version", and "blacklist".
// Only "blacklist" is mandatory.
// 1. "os" contains "type" and an optional "version". "type" could be "macosx",
//    "linux", "win", or "any".  "any" is the same as not specifying "os".
//    "version" is a VERSION structure (defined later).
// 2. "vendor_id" has the value of a string.
// 3. "device_id" has the value of a string.
// 4. "driver_version" is a VERSION structure (defined later).
// 5. "blacklist" is a list of gpu feature strings, valid values include
//    "accelerated_2d_canvas", "accelerated_compositing", "webgl", and "all".
//    Currently whatever feature is selected, the effect is the same as "all",
//    i.e., it's not supported to turn off one GPU feature and not the others.
// VERSION includes "op" "number", and "number2".  "op" can be any of the
// following value: "=", "<", "<=", ">", ">=", "any", "between".  "number2" is
// only used if "op" is "between".  "number" is used for all "op" values except
// "any". "number" and "number2" are in the format of x, x.x, x.x.x, ect.
// Check out "gpu_blacklist_unittest.cc" for examples.

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
                                           const GPUInfo& gpu_info) const;

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

  class GpuBlacklistEntry {
   public:
    // Constructs GpuBlacklistEntry from DictionaryValue loaded from json.
    static GpuBlacklistEntry* GetGpuBlacklistEntryFromValue(
        DictionaryValue* value);

    // Determines if a given os/gc/driver is included in the Entry set.
    bool Contains(OsType os_type, const Version& os_version,
                  uint32 vendor_id, uint32 device_id,
                  const Version& driver_version) const;

    // Returns the OsType.
    OsType GetOsType() const;

    // Returns the GpuFeatureFlags.
    GpuFeatureFlags GetGpuFeatureFlags() const;

   private:
    GpuBlacklistEntry();

    bool SetOsInfo(const std::string& os,
                   const std::string& version_op,
                   const std::string& version_string,
                   const std::string& version_string2);

    bool SetVendorId(const std::string& vendor_id_string);

    bool SetDeviceId(const std::string& device_id_string);

    bool SetDriverVersionInfo(const std::string& version_op,
                              const std::string& version_string,
                              const std::string& version_string2);

    bool SetBlacklistedFeatures(
        const std::vector<std::string>& blacklisted_features);

    scoped_ptr<OsInfo> os_info_;
    uint32 vendor_id_;
    uint32 device_id_;
    scoped_ptr<VersionInfo> driver_version_info_;
    scoped_ptr<GpuFeatureFlags> feature_flags_;
  };

  // Gets the current OS type.
  static OsType GetOsType();

  void Clear();

  std::vector<GpuBlacklistEntry*> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(GpuBlacklist);
};

#endif  // CHROME_BROWSER_GPU_BLACKLIST_H_

