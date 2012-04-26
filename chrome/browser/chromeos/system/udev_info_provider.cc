// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/udev_info_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/system/name_value_pairs_parser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {
namespace system {

namespace {

// Absolute path of the udevadm tool.
const char kUdevadmToolPath[] = "/sbin/udevadm";
// Key-value separator and string delimiter in output of "udevadm info".
const char kUdevadmEq[] = "=";
const char kUdevadmDelim[] = "\n";
const char kUdevadmCommentDelim[] = "";

}  // namespace

class UdevInfoProviderImpl : public UdevInfoProvider {
 public:
  UdevInfoProviderImpl() {}

  // UdevInfoProvider implementation.
  virtual bool QueryDeviceProperty(const std::string& sys_path,
                                   const std::string& property_name,
                                   std::string* result) const OVERRIDE;

  static UdevInfoProvider* GetInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(UdevInfoProviderImpl);
};

base::LazyInstance<UdevInfoProviderImpl> g_udev_info_provider =
    LAZY_INSTANCE_INITIALIZER;

bool UdevInfoProviderImpl::QueryDeviceProperty(const std::string& sys_path,
                                               const std::string& property_name,
                                               std::string* result) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  const char* kUdevadmTool[] = {
    kUdevadmToolPath,
    "info",
    "--path",
    sys_path.c_str(),
    "--query=property",
  };

  NameValuePairsParser::NameValueMap device_info;
  NameValuePairsParser parser(&device_info);

  if (!parser.ParseNameValuePairsFromTool(
          arraysize(kUdevadmTool), kUdevadmTool, kUdevadmEq,
          kUdevadmDelim, kUdevadmCommentDelim)) {
    LOG(WARNING) << "There were errors parsing the output of "
                 << kUdevadmToolPath << ".";
  }

  NameValuePairsParser::NameValueMap::const_iterator it =
      device_info.find(property_name);
  if (it == device_info.end())
    return false;

  *result = it->second;
  return true;
}

UdevInfoProvider* UdevInfoProviderImpl::GetInstance() {
  return &g_udev_info_provider.Get();
}

UdevInfoProvider* UdevInfoProvider::GetInstance() {
  return UdevInfoProviderImpl::GetInstance();
}

}  // namespace system
}  // namespace chromeos
