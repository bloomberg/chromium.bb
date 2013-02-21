// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/media_device_notifications_utils.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/text/bytes_formatting.h"

namespace chrome {

bool IsMediaDevice(const base::FilePath::StringType& mount_point) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::FilePath dcim_path(mount_point);
  base::FilePath::StringType dcim_dir(kDCIMDirectoryName);
  if (!file_util::DirectoryExists(dcim_path.Append(dcim_dir))) {
    // Check for lowercase 'dcim' as well.
    base::FilePath dcim_path_lower(
        dcim_path.Append(StringToLowerASCII(dcim_dir)));
    if (!file_util::DirectoryExists(dcim_path_lower))
      return false;
  }
  return true;
}

string16 GetFullProductName(const std::string& vendor_name,
                            const std::string& model_name) {
  if (vendor_name.empty() && model_name.empty())
    return string16();

  std::string product_name;
  if (vendor_name.empty())
    product_name = model_name;
  else if (model_name.empty())
    product_name = vendor_name;
  else
    product_name = vendor_name + ", " + model_name;
  return IsStringUTF8(product_name) ?
      UTF8ToUTF16("(" + product_name + ")") : string16();
}

string16 GetDisplayNameForDevice(uint64 storage_size_in_bytes,
                                 const string16& name) {
  DCHECK(!name.empty());
  return (storage_size_in_bytes == 0) ?
      name : ui::FormatBytes(storage_size_in_bytes) + ASCIIToUTF16(" ") + name;
}

}  // namespace chrome
