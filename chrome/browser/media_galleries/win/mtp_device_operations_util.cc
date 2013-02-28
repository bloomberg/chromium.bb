// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/win/mtp_device_operations_util.h"

#include <portabledevice.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace media_transfer_protocol {

namespace {

// On success, returns true and updates |client_info| with a reference to an
// IPortableDeviceValues interface that holds information about the
// application that communicates with the device.
bool GetClientInformation(
    base::win::ScopedComPtr<IPortableDeviceValues>* client_info) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(client_info);
  HRESULT hr = client_info->CreateInstance(__uuidof(PortableDeviceValues),
                                           NULL, CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to create an instance of IPortableDeviceValues";
    return false;
  }

  (*client_info)->SetStringValue(WPD_CLIENT_NAME,
                                 chrome::kBrowserProcessExecutableName);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, 0);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, 0);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, 0);
  (*client_info)->SetUnsignedIntegerValue(
      WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS,
                                          GENERIC_READ);
  return true;
}

// Gets the content interface of the portable |device|. On success, returns
// the IPortableDeviceContent interface. On failure, returns NULL.
base::win::ScopedComPtr<IPortableDeviceContent> GetDeviceContent(
    IPortableDevice* device) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(device);
  base::win::ScopedComPtr<IPortableDeviceContent> content;
  if (SUCCEEDED(device->Content(content.Receive())))
    return content;
  return base::win::ScopedComPtr<IPortableDeviceContent>();
}

// On success, returns IEnumPortableDeviceObjectIDs interface to enumerate
// the device objects. On failure, returns NULL.
// |parent_id| specifies the parent object identifier.
base::win::ScopedComPtr<IEnumPortableDeviceObjectIDs> GetDeviceObjectEnumerator(
    IPortableDevice* device,
    const string16& parent_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(device);
  DCHECK(!parent_id.empty());
  base::win::ScopedComPtr<IPortableDeviceContent> content =
      GetDeviceContent(device);
  if (!content)
    return base::win::ScopedComPtr<IEnumPortableDeviceObjectIDs>();

  base::win::ScopedComPtr<IEnumPortableDeviceObjectIDs> enum_object_ids;
  if (SUCCEEDED(content->EnumObjects(0, parent_id.c_str(), NULL,
                                     enum_object_ids.Receive())))
    return enum_object_ids;
  return base::win::ScopedComPtr<IEnumPortableDeviceObjectIDs>();
}

// Writes data from |stream| to the file specified by |local_path|. On success,
// returns true and the stream contents are appended to the file.
// TODO(kmadhusu) Deprecate this function after fixing crbug.com/110119.
bool WriteStreamContentsToFile(IStream* stream,
                               size_t optimal_transfer_size,
                               const base::FilePath& local_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(stream);
  DCHECK(!local_path.empty());
  if (optimal_transfer_size == 0U)
    return false;
  DWORD read = 0;
  HRESULT hr = S_OK;
  std::string buffer;
  do {
    hr = stream->Read(WriteInto(&buffer, optimal_transfer_size + 1),
                      optimal_transfer_size, &read);
    // IStream::Read() returns S_FALSE when the actual number of bytes read from
    // the stream object is less than the number of bytes requested (aka
    // |optimal_transfer_size|). This indicates the end of the stream has been
    // reached. Therefore, it is fine to return true when Read() returns
    // S_FALSE.
    if ((hr != S_OK) && (hr != S_FALSE))
      return false;
    if (read) {
      int data_len = std::min<int>(read, buffer.length());
      if (file_util::AppendToFile(local_path, buffer.c_str(), data_len) !=
              data_len)
        return false;
    }
  } while (read > 0);
  return true;
}

// Returns whether the object is a directory/folder/album. |properties_values|
// contains the object property key values.
bool IsDirectory(IPortableDeviceValues* properties_values) {
  DCHECK(properties_values);
  GUID content_type;
  HRESULT hr = properties_values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE,
                                               &content_type);
  if (FAILED(hr))
    return false;
  // TODO(kmadhusu): |content_type| can be an image or audio or video or mixed
  // album. It is not clear whether an album is a collection of physical objects
  // or virtual objects. Investigate this in detail.

  // The root storage object describes its content type as
  // WPD_CONTENT_FUNCTIONAL_OBJECT.
  return (content_type == WPD_CONTENT_TYPE_FOLDER ||
          content_type == WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT);
}

// Returns the friendly name of the object from the property key values
// specified by the |properties_values|.
string16 GetObjectName(IPortableDeviceValues* properties_values,
                       bool is_directory) {
  DCHECK(properties_values);
  base::win::ScopedCoMem<char16> buffer;
  REFPROPERTYKEY key =
      is_directory ? WPD_OBJECT_NAME : WPD_OBJECT_ORIGINAL_FILE_NAME;
  HRESULT hr = properties_values->GetStringValue(key, &buffer);
  string16 result;
  if (SUCCEEDED(hr))
    result.assign(buffer);
  return result;
}

// Gets the last modified time of the object from the property key values
// specified by the |properties_values|. On success, returns true and fills in
// |last_modified_time|.
bool GetLastModifiedTime(IPortableDeviceValues* properties_values,
                         base::Time* last_modified_time) {
  DCHECK(properties_values);
  DCHECK(last_modified_time);
  base::win::ScopedPropVariant last_modified_date;
  HRESULT hr = properties_values->GetValue(WPD_OBJECT_DATE_MODIFIED,
                                           last_modified_date.Receive());
  if (FAILED(hr))
    return false;

  bool last_modified_time_set = (last_modified_date.get().vt == VT_DATE);
  if (last_modified_time_set) {
    SYSTEMTIME system_time;
    FILETIME file_time;
    if (VariantTimeToSystemTime(last_modified_date.get().date, &system_time) &&
        SystemTimeToFileTime(&system_time, &file_time)) {
      *last_modified_time = base::Time::FromFileTime(file_time);
    } else {
      last_modified_time_set = false;
    }
  }
  return last_modified_time_set;
}

// Gets the size of the file object in bytes from the property key values
// specified by the |properties_values|. On success, returns true and fills
// in |size|.
bool GetObjectSize(IPortableDeviceValues* properties_values, int64* size) {
  DCHECK(properties_values);
  DCHECK(size);
  ULONGLONG actual_size;
  HRESULT hr = properties_values->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE,
                                                               &actual_size);
  bool success = SUCCEEDED(hr) && (actual_size <= kint64max);
  if (success)
    *size = static_cast<int64>(actual_size);
  return success;
}

// Gets the details of the object specified by the |object_id| given the media
// transfer protocol |device|. On success, returns true and fills in |name|,
// |is_directory|, |size| and |last_modified_time|.
bool GetObjectDetails(IPortableDevice* device,
                      const string16 object_id,
                      string16* name,
                      bool* is_directory,
                      int64* size,
                      base::Time* last_modified_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(device);
  DCHECK(!object_id.empty());
  DCHECK(name);
  DCHECK(is_directory);
  DCHECK(size);
  DCHECK(last_modified_time);
  base::win::ScopedComPtr<IPortableDeviceContent> content =
      GetDeviceContent(device);
  if (!content)
    return false;

  base::win::ScopedComPtr<IPortableDeviceProperties> properties;
  HRESULT hr = content->Properties(properties.Receive());
  if (FAILED(hr))
    return false;

  base::win::ScopedComPtr<IPortableDeviceKeyCollection> properties_to_read;
  hr = properties_to_read.CreateInstance(__uuidof(PortableDeviceKeyCollection),
                                         NULL,
                                         CLSCTX_INPROC_SERVER);
  if (FAILED(hr))
    return false;

  if (FAILED(properties_to_read->Add(WPD_OBJECT_CONTENT_TYPE)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_FORMAT)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_ORIGINAL_FILE_NAME)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_NAME)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_DATE_MODIFIED)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_SIZE)))
    return false;

  base::win::ScopedComPtr<IPortableDeviceValues> properties_values;
  hr = properties->GetValues(object_id.c_str(),
                             properties_to_read.get(),
                             properties_values.Receive());
  if (FAILED(hr))
    return false;

  *is_directory = IsDirectory(properties_values.get());
  *name = GetObjectName(properties_values.get(), *is_directory);
  if (name->empty())
    return false;

  if (*is_directory) {
    // Directory entry does not have size and last modified date property key
    // values.
    *size = 0;
    *last_modified_time = base::Time();
    return true;
  }
  return (GetObjectSize(properties_values.get(), size) &&
      GetLastModifiedTime(properties_values.get(), last_modified_time));
}

// Creates an MTP device object entry for the given |device| and |object_id|.
// On success, returns true and fills in |entry|.
bool GetMTPDeviceObjectEntry(IPortableDevice* device,
                             const string16& object_id,
                             MTPDeviceObjectEntry* entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(device);
  DCHECK(!object_id.empty());
  DCHECK(entry);
  string16 name;
  bool is_directory;
  int64 size;
  base::Time last_modified_time;
  if (!GetObjectDetails(device, object_id, &name, &is_directory, &size,
                        &last_modified_time))
    return false;
  *entry = MTPDeviceObjectEntry(object_id, name, is_directory, size,
                                last_modified_time);
  return true;
}

// Gets the entries of the directory specified by |directory_object_id| from
// the given MTP |device|. Set |get_all_entries| to true to get all the entries
// of the directory. To request a specific object entry, put the object name in
// |object_name|. Leave |object_name| blank to request all entries. On
// success returns true and set |object_entries|.
bool GetMTPDeviceObjectEntries(IPortableDevice* device,
                               const string16& directory_object_id,
                               const string16& object_name,
                               MTPDeviceObjectEntries* object_entries) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(device);
  DCHECK(!directory_object_id.empty());
  DCHECK(object_entries);
  base::win::ScopedComPtr<IEnumPortableDeviceObjectIDs> enum_object_ids =
      GetDeviceObjectEnumerator(device, directory_object_id);
  if (!enum_object_ids)
    return false;

  // Loop calling Next() while S_OK is being returned.
  const DWORD num_objects_to_request = 10;
  const bool get_all_entries = object_name.empty();
  for (HRESULT hr = S_OK; hr == S_OK;) {
    DWORD num_objects_fetched = 0;
    scoped_ptr<char16*[]> object_ids(new char16*[num_objects_to_request]);
    hr = enum_object_ids->Next(num_objects_to_request,
                               object_ids.get(),
                               &num_objects_fetched);
    for (DWORD index = 0; index < num_objects_fetched; ++index) {
      MTPDeviceObjectEntry entry;
      if (GetMTPDeviceObjectEntry(device,
                                  object_ids[index],
                                  &entry)) {
        if (get_all_entries) {
          object_entries->push_back(entry);
        } else if (entry.name == object_name) {
          object_entries->push_back(entry);  // Object entry found.
          break;
        }
      }
    }
    for (DWORD index = 0; index < num_objects_fetched; ++index)
      CoTaskMemFree(object_ids[index]);
  }
  return true;
}

}  // namespace

base::win::ScopedComPtr<IPortableDevice> OpenDevice(
    const string16& pnp_device_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!pnp_device_id.empty());
  base::win::ScopedComPtr<IPortableDeviceValues> client_info;
  if (!GetClientInformation(&client_info))
    return base::win::ScopedComPtr<IPortableDevice>();
  base::win::ScopedComPtr<IPortableDevice> device;
  HRESULT hr = device.CreateInstance(__uuidof(PortableDevice), NULL,
                                     CLSCTX_INPROC_SERVER);
  if (FAILED(hr))
    return base::win::ScopedComPtr<IPortableDevice>();

  hr = device->Open(pnp_device_id.c_str(), client_info.get());
  if (SUCCEEDED(hr))
    return device;
  if (hr == E_ACCESSDENIED)
    DPLOG(ERROR) << "Access denied to open the device";
  return base::win::ScopedComPtr<IPortableDevice>();
}

base::PlatformFileError GetFileEntryInfo(
    IPortableDevice* device,
    const string16& object_id,
    base::PlatformFileInfo* file_entry_info) {
  DCHECK(device);
  DCHECK(!object_id.empty());
  DCHECK(file_entry_info);
  MTPDeviceObjectEntry entry;
  if (!GetMTPDeviceObjectEntry(device, object_id, &entry))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  file_entry_info->size = entry.size;
  file_entry_info->is_directory = entry.is_directory;
  file_entry_info->is_symbolic_link = false;
  file_entry_info->last_modified = entry.last_modified_time;
  file_entry_info->last_accessed = entry.last_modified_time;
  file_entry_info->creation_time = base::Time();
  return base::PLATFORM_FILE_OK;
}

bool GetDirectoryEntries(IPortableDevice* device,
                         const string16& directory_object_id,
                         MTPDeviceObjectEntries* object_entries) {
  return GetMTPDeviceObjectEntries(device, directory_object_id, string16(),
                                   object_entries);
}

bool WriteFileObjectContentToPath(IPortableDevice* device,
                                  const string16& file_object_id,
                                  const base::FilePath& local_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(device);
  DCHECK(!file_object_id.empty());
  DCHECK(!local_path.empty());
  base::win::ScopedComPtr<IPortableDeviceContent> content =
      GetDeviceContent(device);
  if (!content)
    return false;

  base::win::ScopedComPtr<IPortableDeviceResources> resources;
  HRESULT hr = content->Transfer(resources.Receive());
  if (FAILED(hr))
    return false;

  base::win::ScopedComPtr<IStream> file_stream;
  DWORD optimal_transfer_size = 0;
  hr = resources->GetStream(file_object_id.c_str(), WPD_RESOURCE_DEFAULT,
                            STGM_READ, &optimal_transfer_size,
                            file_stream.Receive());
  if (FAILED(hr))
    return false;
  return WriteStreamContentsToFile(file_stream.get(), optimal_transfer_size,
                                   local_path);
}

string16 GetObjectIdFromName(IPortableDevice* device,
                             const string16& parent_id,
                             const string16& object_name) {
  MTPDeviceObjectEntries object_entries;
  if (!GetMTPDeviceObjectEntries(device, parent_id, object_name,
                                 &object_entries) ||
      object_entries.empty())
    return string16();
  // TODO(thestig): This DCHECK can fail. Multiple MTP objects can have
  // the same name. Handle the situation gracefully. Refer to crbug.com/169930
  // for more details.
  DCHECK_EQ(1U, object_entries.size());
  return object_entries[0].object_id;
}

}  // namespace media_transfer_protocol

}  // namespace chrome
