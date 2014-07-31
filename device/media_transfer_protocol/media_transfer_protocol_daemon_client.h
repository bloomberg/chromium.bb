// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Client code to talk to the Media Transfer Protocol daemon. The MTP daemon is
// responsible for communicating with PTP / MTP capable devices like cameras
// and smartphones.

#ifndef DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
#define DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "build/build_config.h"

#if !defined(OS_LINUX)
#error "Only used on Linux and ChromeOS"
#endif

class MtpFileEntry;
class MtpStorageInfo;

namespace dbus {
class Bus;
}

namespace device {

// A class to make the actual DBus calls for mtpd service.
// This class only makes calls, result/error handling should be done
// by callbacks.
class MediaTransferProtocolDaemonClient {
 public:
  // A callback to be called when DBus method call fails.
  typedef base::Closure ErrorCallback;

  // A callback to handle the result of EnumerateAutoMountableDevices.
  // The argument is the enumerated storage names.
  typedef base::Callback<void(const std::vector<std::string>& storage_names)
                         > EnumerateStoragesCallback;

  // A callback to handle the result of GetStorageInfo.
  // The argument is the information about the specified storage.
  typedef base::Callback<void(const MtpStorageInfo& storage_info)
                         > GetStorageInfoCallback;

  // A callback to handle the result of OpenStorage.
  // The argument is the returned handle.
  typedef base::Callback<void(const std::string& handle)> OpenStorageCallback;

  // A callback to handle the result of CloseStorage.
  typedef base::Closure CloseStorageCallback;

  // A callback to handle the result of ReadDirectoryEntryIds.
  // The argument is a vector of file ids.
  typedef base::Callback<void(const std::vector<uint32>& file_ids)
                         > ReadDirectoryEntryIdsCallback;

  // A callback to handle the result of GetFileInfo.
  // The argument is a vector of file entries.
  typedef base::Callback<void(const std::vector<MtpFileEntry>& file_entries)
                         > GetFileInfoCallback;

  // A callback to handle the result of ReadFileChunkById.
  // The argument is a string containing the file data.
  typedef base::Callback<void(const std::string& data)> ReadFileCallback;

  // A callback to handle storage attach/detach events.
  // The first argument is true for attach, false for detach.
  // The second argument is the storage name.
  typedef base::Callback<void(bool is_attach,
                              const std::string& storage_name)
                         > MTPStorageEventHandler;

  virtual ~MediaTransferProtocolDaemonClient();

  // Calls EnumerateStorages method. |callback| is called after the
  // method call succeeds, otherwise, |error_callback| is called.
  virtual void EnumerateStorages(
      const EnumerateStoragesCallback& callback,
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
                           const std::string& mode,
                           const OpenStorageCallback& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls CloseStorage method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |handle| comes from a OpenStorageCallback.
  virtual void CloseStorage(const std::string& handle,
                            const CloseStorageCallback& callback,
                            const ErrorCallback& error_callback) = 0;

  // Calls ReadDirectoryEntryIds method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  virtual void ReadDirectoryEntryIds(
      const std::string& handle,
      uint32 file_id,
      const ReadDirectoryEntryIdsCallback& callback,
      const ErrorCallback& error_callback) = 0;

  // Calls GetFileInfo method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  // |file_ids| is a list of MTP-device specific file ids.
  // |offset| is the index into |file_ids| to read from.
  // |entries_to_read| is the maximum number of file entries to read.
  virtual void GetFileInfo(const std::string& handle,
                           const std::vector<uint32>& file_ids,
                           size_t offset,
                           size_t entries_to_read,
                           const GetFileInfoCallback& callback,
                           const ErrorCallback& error_callback) = 0;

  // TODO(thestig): Rename to ReadFileChunk.
  // Calls ReadFilePathById method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  // |bytes_to_read| cannot exceed 1 MiB.
  virtual void ReadFileChunkById(const std::string& handle,
                                 uint32 file_id,
                                 uint32 offset,
                                 uint32 bytes_to_read,
                                 const ReadFileCallback& callback,
                                 const ErrorCallback& error_callback) = 0;

  // Registers given callback for events. Should only be called once.
  // |storage_event_handler| is called when a mtp storage attach or detach
  // signal is received.
  virtual void ListenForChanges(const MTPStorageEventHandler& handler) = 0;

  // Factory function, creates a new instance and returns ownership.
  static MediaTransferProtocolDaemonClient* Create(dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  MediaTransferProtocolDaemonClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDaemonClient);
};

}  // namespace device

#endif  // DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
