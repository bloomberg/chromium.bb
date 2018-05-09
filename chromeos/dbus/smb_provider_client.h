// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SMB_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_SMB_PROVIDER_CLIENT_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/smbprovider/directory_entry.pb.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// SmbProviderClient is used to communicate with the org.chromium.SmbProvider
// service. All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT SmbProviderClient
    : public DBusClient,
      public base::SupportsWeakPtr<SmbProviderClient> {
 public:
  using GetMetdataEntryCallback =
      base::OnceCallback<void(smbprovider::ErrorType error,
                              const smbprovider::DirectoryEntryProto& entry)>;
  using MountCallback =
      base::OnceCallback<void(smbprovider::ErrorType error, int32_t mount_id)>;
  using OpenFileCallback =
      base::OnceCallback<void(smbprovider::ErrorType error, int32_t file_id)>;
  using ReadDirectoryCallback = base::OnceCallback<void(
      smbprovider::ErrorType error,
      const smbprovider::DirectoryEntryListProto& entries)>;
  using StatusCallback = base::OnceCallback<void(smbprovider::ErrorType error)>;
  using ReadFileCallback = base::OnceCallback<void(smbprovider::ErrorType error,
                                                   const base::ScopedFD& fd)>;
  using GetDeleteListCallback =
      base::OnceCallback<void(smbprovider::ErrorType error,
                              const smbprovider::DeleteListProto& delete_list)>;
  using SetupKerberosCallback = base::OnceCallback<void(bool success)>;

  ~SmbProviderClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static SmbProviderClient* Create();

  // Calls Mount. It runs OpenDirectory() on |share_path| to check that it is a
  // valid share. |workgroup|, |username|, and |password_fd| will be used as
  // credentials to access the mount. |callback| is called after getting (or
  // failing to get) D-BUS response.
  virtual void Mount(const base::FilePath& share_path,
                     const std::string& workgroup,
                     const std::string& username,
                     base::ScopedFD password_fd,
                     MountCallback callback) = 0;

  // Calls Remount. This attempts to remount the share at |share_path| with its
  // original |mount_id|.
  virtual void Remount(const base::FilePath& share_path,
                       int32_t mount_id,
                       StatusCallback callback) = 0;

  // Calls Unmount. This removes the corresponding mount of |mount_id| from
  // the list of valid mounts. Subsequent operations on |mount_id| will fail.
  virtual void Unmount(int32_t mount_id, StatusCallback callback) = 0;

  // Calls ReadDirectory. Using the corresponding mount of |mount_id|, this
  // reads the directory on a given |directory_path| and passes the
  // DirectoryEntryList to the supplied ReadDirectoryCallback.
  virtual void ReadDirectory(int32_t mount_id,
                             const base::FilePath& directory_path,
                             ReadDirectoryCallback callback) = 0;

  // Calls GetMetadataEntry. Using the corresponding mount of |mount_id|, this
  // reads an entry in a given |entry_path| and passes the DirectoryEntry to the
  // supplied GetMetadataEntryCallback.
  virtual void GetMetadataEntry(int32_t mount_id,
                                const base::FilePath& entry_path,
                                GetMetdataEntryCallback callback) = 0;

  // Calls OpenFile. Using the corresponding mount |mount_id|, this opens the
  // file at a given |file_path|, and passes a file handle to the supplied
  // OpenFileCallback.
  virtual void OpenFile(int32_t mount_id,
                        const base::FilePath& file_path,
                        bool writeable,
                        OpenFileCallback callback) = 0;

  // Calls CloseFile. This closes the file with mount |mount_id| and handle
  // |file_id|. Subsequent operations using file with this handle will fail.
  virtual void CloseFile(int32_t mount_id,
                         int32_t file_id,
                         StatusCallback callback) = 0;

  // Calls ReadFile. Using the corresponding mount |mount_id|, this reads the
  // file with handle |file_id| from |offset| and reads up to |length| in bytes.
  // The data read is saved to a temporary file and is returned as a file
  // descriptor in the supplied ReadFileCallback.
  virtual void ReadFile(int32_t mount_id,
                        int32_t file_id,
                        int64_t offset,
                        int32_t length,
                        ReadFileCallback callback) = 0;

  // Calls DeleteEntry. This deletes the file or directory at |entry_path|.
  // Subsequent operations on the entry at this path will fail.
  virtual void DeleteEntry(int32_t mount_id,
                           const base::FilePath& entry_path,
                           bool recursive,
                           StatusCallback callback) = 0;

  // Calls CreateFile. Using the corresponding mount |mount_id|, this creates
  // the file in the specified |file_path|.
  virtual void CreateFile(int32_t mount_id,
                          const base::FilePath& file_path,
                          StatusCallback callback) = 0;

  // Calls Truncate. Using the corresponding mount |mount_id|, this truncates
  // the file in |file_path| to the desired |length|.
  virtual void Truncate(int32_t mount_id,
                        const base::FilePath& file_path,
                        int64_t length,
                        StatusCallback callback) = 0;

  // Calls WriteFile. Using the corresponding mount |mount_id|, this writes to a
  // file with handle |file_id| from |offset| and writes |length| bytes. The
  // data to be written is contained in the file with handle |temp_fd|.
  virtual void WriteFile(int32_t mount_id,
                         int32_t file_id,
                         int64_t offset,
                         int32_t length,
                         base::ScopedFD temp_fd,
                         StatusCallback callback) = 0;

  // Calls CreateDirectory. Using the corresponding |mount_id|, this creates the
  // directory at |directory_path|. If |recursive| is set to true, this creates
  // all non-existing directories on the path. The operation will fail if the
  // directory already exists.
  virtual void CreateDirectory(int32_t mount_id,
                               const base::FilePath& directory_path,
                               bool recursive,
                               StatusCallback callback) = 0;

  // Calls MoveEntry. Using the corresponding |mount_id|, this moves the entry
  // at |source_path| to |target_path|. This operation will fail if the
  // target already exists.
  virtual void MoveEntry(int32_t mount_id,
                         const base::FilePath& source_path,
                         const base::FilePath& target_path,
                         StatusCallback callback) = 0;

  // Calls CopyEntry. Using the corresponding |mount_id|, this copies the entry
  // at |source_path| to |target_path|. This operation will fail if the
  // target already exists.
  virtual void CopyEntry(int32_t mount_id,
                         const base::FilePath& source_path,
                         const base::FilePath& target_path,
                         StatusCallback callback) = 0;

  // Calls GetDeleteList. Using the corresponding |mount_id|, this generates an
  // ordered list of individual entries that must be deleted in order to delete
  // |entry_path|. This operations does not modify the filesystem.
  virtual void GetDeleteList(int32_t mount_id,
                             const base::FilePath& entry_path,
                             GetDeleteListCallback callback) = 0;

  // Calls GetShares. This gets the shares from |server_url| and calls
  // |callback| when shares are found. The DirectoryEntryListProto will contain
  // no entries if there are no shares found.
  virtual void GetShares(const base::FilePath& server_url,
                         ReadDirectoryCallback callback) = 0;

  // Calls SetupKerberos. This sets up Kerberos for the user |account_id|,
  // fetching the user's Kerberos files from AuthPolicy. The user must be
  // ChromAD enrolled.
  virtual void SetupKerberos(const std::string& account_id,
                             SetupKerberosCallback callback) = 0;

 protected:
  // Create() should be used instead.
  SmbProviderClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbProviderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SMB_PROVIDER_CLIENT_H_
