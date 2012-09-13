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

class MtpFileEntry;
class MtpStorageInfo;

namespace dbus {
class Bus;
}

namespace chromeos {

// Mode to open a storage in.
enum OpenStorageMode {
  OPEN_STORAGE_MODE_READ_ONLY,
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
  typedef base::Callback<void(const MtpStorageInfo& storage_info)
                         > GetStorageInfoCallback;

  // A callback to handle the result of OpenStorage.
  // The argument is the returned handle.
  typedef base::Callback<void(const std::string& handle)> OpenStorageCallback;

  // A callback to handle the result of CloseStorage.
  typedef base::Callback<void()> CloseStorageCallback;

  // A callback to handle the result of ReadDirectoryByPath/Id.
  // The argument is a vector of file entries.
  typedef base::Callback<void(const std::vector<MtpFileEntry>& file_entries)
                         > ReadDirectoryCallback;

  // A callback to handle the result of ReadFileByPath/Id.
  // The argument is a string containing the file data.
  // TODO(thestig) Consider using a file descriptor instead of the data.
  typedef base::Callback<void(const std::string& data)> ReadFileCallback;

  // A callback to handle the result of GetFileInfoByPath/Id.
  // The argument is a file entry.
  typedef base::Callback<void(const MtpFileEntry& file_entry)
                         > GetFileInfoCallback;

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

  // Calls GetFileInfoByPath method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  virtual void GetFileInfoByPath(const std::string& handle,
                                 const std::string& path,
                                 const GetFileInfoCallback& callback,
                                 const ErrorCallback& error_callback) = 0;

  // Calls GetFileInfoById method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  virtual void GetFileInfoById(const std::string& handle,
                               uint32 file_id,
                               const GetFileInfoCallback& callback,
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
