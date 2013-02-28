// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has several utility functions to open a media transfer protocol
// (MTP) device for communication, to enumerate the device contents, to read the
// device file object, etc. All these tasks may take an arbitary long time
// to complete. This file segregates those functionalities and runs them
// in the blocking pool thread rather than in the UI thread.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OPERATIONS_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OPERATIONS_UTIL_H_

#include <portabledeviceapi.h>

#include <string>

#include "base/platform_file.h"
#include "base/string16.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"

namespace chrome {

namespace media_transfer_protocol {

// Opens the device for communication. |pnp_device_id| specifies the plug and
// play device ID string. On success, returns the IPortableDevice interface.
// On failure, returns NULL.
base::win::ScopedComPtr<IPortableDevice> OpenDevice(
    const string16& pnp_device_id);

// Gets the details of the object specified by |object_id| from the given MTP
// |device|. On success, returns no error (base::PLATFORM_FILE_OK) and fills in
// |file_entry_info|. On failure, returns the corresponding platform file error
// and |file_entry_info| is not set.
base::PlatformFileError GetFileEntryInfo(
    IPortableDevice* device,
    const string16& object_id,
    base::PlatformFileInfo* file_entry_info);

// Gets the entries of the directory specified by |directory_object_id| from
// the given MTP |device|. On success, returns true and fills in
// |object_entries|. On failure, returns false and |object_entries| is not
// set.
bool GetDirectoryEntries(IPortableDevice* device,
                         const string16& directory_object_id,
                         MTPDeviceObjectEntries* object_entries);

// Writes the data of the object specified by |file_object_id| from the given
// MTP |device| to the file specified by |local_path|. On success, returns
// true and writes the object data in |local_path|. On failure, returns false.
bool WriteFileObjectContentToPath(IPortableDevice* device,
                                  const string16& file_object_id,
                                  const base::FilePath& local_path);

// Returns the identifier of the object specified by the |object_name|.
// |parent_id| specifies the object's parent identifier.
// |object_name| specifies the friendly name of the object.
string16 GetObjectIdFromName(IPortableDevice* device,
                             const string16& parent_id,
                             const string16& object_name);

}  // namespace media_transfer_protocol

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OPERATIONS_UTIL_H_
