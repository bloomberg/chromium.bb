// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Client code to talk to the Media Transfer Protocol daemon. The MTP daemon is
// responsible for communicating with PTP / MTP capable devices like cameras
// and smartphones.

#ifndef CHROMEOS_DBUS_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace dbus {
class Bus;
class Response;
}

namespace chromeos {

// Mode to open a storage in.
enum OpenStorageMode {
  OPEN_STORAGE_MODE_READ_ONLY,
};

// Values match libmtp values unless noted below.
// TODO(thestig) See if we can do better than this.
enum FileType {
  FILE_TYPE_FOLDER = 0,
  FILE_TYPE_JPEG = 14,
  FILE_TYPE_JFIF = 15,
  FILE_TYPE_TIFF = 16,
  FILE_TYPE_BMP = 17,
  FILE_TYPE_GIF = 18,
  FILE_TYPE_PICT = 19,
  FILE_TYPE_PNG = 20,
  FILE_TYPE_WINDOWSIMAGEFORMAT = 25,
  FILE_TYPE_JP2 = 40,
  FILE_TYPE_JPX = 41,
  // Truly unknown file type.
  FILE_TYPE_UNKNOWN = 44,
  // There's more file types to map to, but right now they are not interesting.
  // Just assign a dummy value for now.
  FILE_TYPE_OTHER = 9999
};

// A class to represent information about a storage sent from mtpd.
class CHROMEOS_EXPORT StorageInfo {
 public:
  StorageInfo();
  StorageInfo(const std::string& storage_name, dbus::Response* response);
  ~StorageInfo();

  // Storage name. (e.g. usb:1,5:65537)
  const std::string& storage_name() const { return storage_name_; }

  // Vendor. (e.g. Kodak)
  const std::string& vendor() const { return vendor_; }

  // Vendor ID. (e.g. 0x040a)
  uint16 vendor_id() const { return vendor_id_; }

  // Product. (e.g. DC4800)
  const std::string& product() const { return product_; }

  // Vendor ID. (e.g. 0x0160)
  uint16 product_id() const { return product_id_; }

  // Device flags as defined by libmtp.
  uint32 device_flags() const { return device_flags_; }

  // Storage type as defined in libmtp. (e.g. PTP_ST_FixedROM)
  uint16 storage_type() const { return storage_type_; }

  // File system type as defined in libmtp. (e.g. PTP_FST_DCF)
  uint16 filesystem_type() const { return filesystem_type_; }

  // Access capability as defined in libmtp. (e.g. PTP_AC_ReadWrite)
  uint16 access_capability() const { return access_capability_; }

  // Max capacity in bytes.
  uint64 max_capacity() const { return max_capacity_; }

  // Free space in byte.
  uint64 free_space_in_bytes() const { return free_space_in_bytes_; }

  // Free space in number of objects.
  uint64 free_space_in_objects() const { return free_space_in_objects_; }

  // Storage description. (e.g. internal memory)
  const std::string& storage_description() const {
    return storage_description_;
  }

  // Volume identifier. (e.g. the serial number, should be unique)
  const std::string& volume_identifier() const { return volume_identifier_; }

 private:
  void InitializeFromResponse(dbus::Response* response);

  // Device info. (A device can have multiple storages)
  std::string vendor_;
  uint16 vendor_id_;
  std::string product_;
  uint16 product_id_;
  uint32 device_flags_;

  // Storage info.
  std::string storage_name_;
  uint16 storage_type_;
  uint16 filesystem_type_;
  uint16 access_capability_;
  uint64 max_capacity_;
  uint64 free_space_in_bytes_;
  uint64 free_space_in_objects_;
  std::string storage_description_;
  std::string volume_identifier_;
};

// A class to represent information about a file entry sent from mtpd.
class CHROMEOS_EXPORT FileEntry {
 public:
  FileEntry();
  explicit FileEntry(dbus::Response* response);
  ~FileEntry();

  // ID for the file.
  uint32 item_id() const { return item_id_; }

  // ID for the file's parent.
  uint32 parent_id() const { return parent_id_; }

  // Name of the file.
  const std::string& file_name() const { return file_name_; }

  // Size of the file.
  uint64 file_size() const { return file_size_; }

  // Modification time of the file.
  base::Time modification_date() const { return modification_date_; }

  // File type.
  FileType file_type() const { return file_type_; }

 private:
  void InitializeFromResponse(dbus::Response* response);

  // Storage info.
  uint32 item_id_;
  uint32 parent_id_;
  std::string file_name_;
  uint64 file_size_;
  base::Time modification_date_;
  FileType file_type_;
};

// A class to make the actual DBus calls for mtpd service.
// This class only makes calls, result/error handling should be done
// by callbacks.
class CHROMEOS_EXPORT MediaTransferProtocolDaemonClient {
 public:
  // A callback to be called when DBus method call fails.
  typedef base::Callback<void()> ErrorCallback;

  // A callback to handle the result of EnumerateAutoMountableDevices.
  // The argument is the enumerated storage names.
  typedef base::Callback<void(const std::vector<std::string>& storage_names)
                         > EnumerateStorageCallback;

  // A callback to handle the result of GetStorageInfo.
  // The argument is the information about the specified storage.
  typedef base::Callback<void(const StorageInfo& storage_info)
                         > GetStorageInfoCallback;

  // A callback to handle the result of OpenStorage.
  // The argument is the returned handle.
  typedef base::Callback<void(const std::string& handle)> OpenStorageCallback;

  // A callback to handle the result of CloseStorage.
  typedef base::Callback<void()> CloseStorageCallback;

  // A callback to handle the result of ReadDirectoryByPath/Id.
  // The argument is a vector of file entries.
  typedef base::Callback<void(const std::vector<FileEntry>& file_entries)
                         > ReadDirectoryCallback;

  // A callback to handle the result of ReadFileByPath/Id.
  // The argument is a string containing the file data.
  // TODO(thestig) Consider using a file descriptor instead of the data.
  typedef base::Callback<void(const std::string& data)> ReadFileCallback;

  // A callback to handle storage attach/detach events.
  // The first argument is true for attach, false for detach.
  // The second argument is the storage name.
  typedef base::Callback<void(bool is_attach,
                              const std::string& storage_name)
                         > MTPStorageEventHandler;

  virtual ~MediaTransferProtocolDaemonClient();

  // Calls EnumerateStorage method. |callback| is called after the
  // method call succeeds, otherwise, |error_callback| is called.
  virtual void EnumerateStorage(
      const EnumerateStorageCallback& callback,
      const ErrorCallback& error_callback) = 0;

  // Calls GetStorageInfo method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  virtual void GetStorageInfo(const std::string& storage_name,
                              const GetStorageInfoCallback& callback,
                              const ErrorCallback& error_callback) = 0;

  // Calls OpenStorage method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // OpenStorage returns a handle in |callback|.
  virtual void OpenStorage(const std::string& storage_name,
                           OpenStorageMode mode,
                           const OpenStorageCallback& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls CloseStorage method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |handle| comes from a OpenStorageCallback.
  virtual void CloseStorage(const std::string& handle,
                            const CloseStorageCallback& callback,
                            const ErrorCallback& error_callback) = 0;

  // Calls ReadDirectoryByPath method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  virtual void ReadDirectoryByPath(const std::string& handle,
                                   const std::string& path,
                                   const ReadDirectoryCallback& callback,
                                   const ErrorCallback& error_callback) = 0;

  // Calls ReadDirectoryById method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  virtual void ReadDirectoryById(const std::string& handle,
                                 uint32 file_id,
                                 const ReadDirectoryCallback& callback,
                                 const ErrorCallback& error_callback) = 0;

  // Calls ReadFileByPath method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  virtual void ReadFileByPath(const std::string& handle,
                              const std::string& path,
                              const ReadFileCallback& callback,
                              const ErrorCallback& error_callback) = 0;

  // Calls ReadFileById method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  virtual void ReadFileById(const std::string& handle,
                            uint32 file_id,
                            const ReadFileCallback& callback,
                            const ErrorCallback& error_callback) = 0;

  // Registers given callback for events.
  // |storage_event_handler| is called when a mtp storage attach or detach
  // signal is received.
  virtual void SetUpConnections(const MTPStorageEventHandler& handler) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static MediaTransferProtocolDaemonClient*
      Create(DBusClientImplementationType type, dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  MediaTransferProtocolDaemonClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
