// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

#include <map>
#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "device/media_transfer_protocol/media_transfer_protocol_daemon_client.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#else
#include "dbus/bus.h"
#endif

namespace device {

namespace {

MediaTransferProtocolManager* g_media_transfer_protocol_manager = NULL;

// The MediaTransferProtocolManager implementation.
class MediaTransferProtocolManagerImpl : public MediaTransferProtocolManager {
 public:
  MediaTransferProtocolManagerImpl(
      scoped_refptr<base::MessageLoopProxy> loop_proxy)
      : weak_ptr_factory_(this) {
    dbus::Bus* bus = NULL;
#if defined(OS_CHROMEOS)
    DCHECK(!loop_proxy.get());
    chromeos::DBusThreadManager* dbus_thread_manager =
        chromeos::DBusThreadManager::Get();
    bus = dbus_thread_manager->GetSystemBus();
    if (!bus)
      return;
#else
    DCHECK(loop_proxy.get());
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = loop_proxy;
    session_bus_ = new dbus::Bus(options);
    bus = session_bus_.get();
#endif

    DCHECK(bus);
    mtp_client_.reset(
        MediaTransferProtocolDaemonClient::Create(bus, false /* not stub */));

    // Set up signals and start initializing |storage_info_map_|.
    mtp_client_->SetUpConnections(
        base::Bind(&MediaTransferProtocolManagerImpl::OnStorageChanged,
                   weak_ptr_factory_.GetWeakPtr()));
    mtp_client_->EnumerateStorages(
        base::Bind(&MediaTransferProtocolManagerImpl::OnEnumerateStorages,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&base::DoNothing));
  }

  virtual ~MediaTransferProtocolManagerImpl() {
  }

  // MediaTransferProtocolManager override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // MediaTransferProtocolManager override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // MediaTransferProtocolManager override.
  virtual const std::vector<std::string> GetStorages() const OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    std::vector<std::string> storages;
    for (StorageInfoMap::const_iterator it = storage_info_map_.begin();
         it != storage_info_map_.end();
         ++it) {
      storages.push_back(it->first);
    }
    return storages;
  }

  // MediaTransferProtocolManager override.
  virtual const MtpStorageInfo* GetStorageInfo(
      const std::string& storage_name) const OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    StorageInfoMap::const_iterator it = storage_info_map_.find(storage_name);
    if (it == storage_info_map_.end())
      return NULL;
    return &it->second;
  }

  // MediaTransferProtocolManager override.
  virtual void OpenStorage(const std::string& storage_name,
                           const std::string& mode,
                           const OpenStorageCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(storage_info_map_, storage_name)) {
      callback.Run("", true);
      return;
    }
    open_storage_callbacks_.push(callback);
    mtp_client_->OpenStorage(
        storage_name,
        mode,
        base::Bind(&MediaTransferProtocolManagerImpl::OnOpenStorage,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnOpenStorageError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  virtual void CloseStorage(const std::string& storage_handle,
                            const CloseStorageCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(true);
      return;
    }
    close_storage_callbacks_.push(std::make_pair(callback, storage_handle));
    mtp_client_->CloseStorage(
        storage_handle,
        base::Bind(&MediaTransferProtocolManagerImpl::OnCloseStorage,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnCloseStorageError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  virtual void ReadDirectoryByPath(
      const std::string& storage_handle,
      const std::string& path,
      const ReadDirectoryCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(std::vector<MtpFileEntry>(), true);
      return;
    }
    read_directory_callbacks_.push(callback);
    mtp_client_->ReadDirectoryByPath(
        storage_handle,
        path,
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectory,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectoryError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  virtual void ReadDirectoryById(
      const std::string& storage_handle,
      uint32 file_id,
      const ReadDirectoryCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(std::vector<MtpFileEntry>(), true);
      return;
    }
    read_directory_callbacks_.push(callback);
    mtp_client_->ReadDirectoryById(
        storage_handle,
        file_id,
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectory,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectoryError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  virtual void ReadFileChunkByPath(const std::string& storage_handle,
                                   const std::string& path,
                                   uint32 offset,
                                   uint32 count,
                                   const ReadFileCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(std::string(), true);
      return;
    }
    read_file_callbacks_.push(callback);
    mtp_client_->ReadFileChunkByPath(
        storage_handle, path, offset, count,
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadFile,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadFileError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  virtual void ReadFileChunkById(const std::string& storage_handle,
                                 uint32 file_id,
                                 uint32 offset,
                                 uint32 count,
                                 const ReadFileCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(std::string(), true);
      return;
    }
    read_file_callbacks_.push(callback);
    mtp_client_->ReadFileChunkById(
        storage_handle, file_id, offset, count,
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadFile,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadFileError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void GetFileInfoByPath(const std::string& storage_handle,
                                 const std::string& path,
                                 const GetFileInfoCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(MtpFileEntry(), true);
      return;
    }
    get_file_info_callbacks_.push(callback);
    mtp_client_->GetFileInfoByPath(
        storage_handle,
        path,
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetFileInfoError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void GetFileInfoById(const std::string& storage_handle,
                               uint32 file_id,
                               const GetFileInfoCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle)) {
      callback.Run(MtpFileEntry(), true);
      return;
    }
    get_file_info_callbacks_.push(callback);
    mtp_client_->GetFileInfoById(
        storage_handle,
        file_id,
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetFileInfoError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Map of storage names to storage info.
  typedef std::map<std::string, MtpStorageInfo> StorageInfoMap;
  // Callback queues - DBus communication is in-order, thus callbacks are
  // received in the same order as the requests.
  typedef std::queue<OpenStorageCallback> OpenStorageCallbackQueue;
  // (callback, handle)
  typedef std::queue<std::pair<CloseStorageCallback, std::string>
                    > CloseStorageCallbackQueue;
  typedef std::queue<ReadDirectoryCallback> ReadDirectoryCallbackQueue;
  typedef std::queue<ReadFileCallback> ReadFileCallbackQueue;
  typedef std::queue<GetFileInfoCallback> GetFileInfoCallbackQueue;

  void OnStorageChanged(bool is_attach, const std::string& storage_name) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (is_attach) {
      mtp_client_->GetStorageInfo(
          storage_name,
          base::Bind(&MediaTransferProtocolManagerImpl::OnGetStorageInfo,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&base::DoNothing));
      return;
    }

    // Detach case.
    StorageInfoMap::iterator it = storage_info_map_.find(storage_name);
    if (it == storage_info_map_.end()) {
      // This might happen during initialization when |storage_info_map_| has
      // not been fully populated yet?
      return;
    }
    storage_info_map_.erase(it);
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      StorageChanged(false /* detach */, storage_name));
  }

  void OnEnumerateStorages(const std::vector<std::string>& storage_names) {
    DCHECK(thread_checker_.CalledOnValidThread());
    for (size_t i = 0; i < storage_names.size(); ++i) {
      mtp_client_->GetStorageInfo(
          storage_names[i],
          base::Bind(&MediaTransferProtocolManagerImpl::OnGetStorageInfo,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&base::DoNothing));
    }
  }

  void OnGetStorageInfo(const MtpStorageInfo& storage_info) {
    DCHECK(thread_checker_.CalledOnValidThread());
    const std::string& storage_name = storage_info.storage_name();
    if (ContainsKey(storage_info_map_, storage_name)) {
      // This should not happen, since MediaTransferProtocolManagerImpl should
      // only call EnumerateStorages() once, which populates |storage_info_map_|
      // with the already-attached devices.
      // After that, all incoming signals are either for new storage
      // attachments, which should not be in |storage_info_map_|, or for
      // storage detachements, which do not add to |storage_info_map_|.
      NOTREACHED();
      return;
    }

    // New storage. Add it and let the observers know.
    storage_info_map_.insert(std::make_pair(storage_name, storage_info));
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      StorageChanged(true /* is attach */, storage_name));
  }

  void OnOpenStorage(const std::string& handle) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, handle)) {
      handles_.insert(handle);
      open_storage_callbacks_.front().Run(handle, false);
    } else {
      NOTREACHED();
      open_storage_callbacks_.front().Run("", true);
    }
    open_storage_callbacks_.pop();
  }

  void OnOpenStorageError() {
    open_storage_callbacks_.front().Run("", true);
    open_storage_callbacks_.pop();
  }

  void OnCloseStorage() {
    DCHECK(thread_checker_.CalledOnValidThread());
    const std::string& handle = close_storage_callbacks_.front().second;
    if (ContainsKey(handles_, handle)) {
      handles_.erase(handle);
      close_storage_callbacks_.front().first.Run(false);
    } else {
      NOTREACHED();
      close_storage_callbacks_.front().first.Run(true);
    }
    close_storage_callbacks_.pop();
  }

  void OnCloseStorageError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    close_storage_callbacks_.front().first.Run(true);
    close_storage_callbacks_.pop();
  }

  void OnReadDirectory(const std::vector<MtpFileEntry>& file_entries) {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_directory_callbacks_.front().Run(file_entries, false);
    read_directory_callbacks_.pop();
  }

  void OnReadDirectoryError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_directory_callbacks_.front().Run(std::vector<MtpFileEntry>(), true);
    read_directory_callbacks_.pop();
  }

  void OnReadFile(const std::string& data) {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_file_callbacks_.front().Run(data, false);
    read_file_callbacks_.pop();
  }

  void OnReadFileError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_file_callbacks_.front().Run(std::string(), true);
    read_file_callbacks_.pop();
  }

  void OnGetFileInfo(const MtpFileEntry& entry) {
    DCHECK(thread_checker_.CalledOnValidThread());
    get_file_info_callbacks_.front().Run(entry, false);
    get_file_info_callbacks_.pop();
  }

  void OnGetFileInfoError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    get_file_info_callbacks_.front().Run(MtpFileEntry(), true);
    get_file_info_callbacks_.pop();
  }

  // Mtpd DBus client.
  scoped_ptr<MediaTransferProtocolDaemonClient> mtp_client_;

#if !defined(OS_CHROMEOS)
  // And a D-Bus session for talking to mtpd.
  scoped_refptr<dbus::Bus> session_bus_;
#endif

  // Device attachment / detachment observers.
  ObserverList<Observer> observers_;

  base::WeakPtrFactory<MediaTransferProtocolManagerImpl> weak_ptr_factory_;

  // Everything below is only accessed on the UI thread.

  // Map to keep track of attached storages by name.
  StorageInfoMap storage_info_map_;

  // Set of open storage handles.
  std::set<std::string> handles_;

  // Queued callbacks.
  OpenStorageCallbackQueue open_storage_callbacks_;
  CloseStorageCallbackQueue close_storage_callbacks_;
  ReadDirectoryCallbackQueue read_directory_callbacks_;
  ReadFileCallbackQueue read_file_callbacks_;
  GetFileInfoCallbackQueue get_file_info_callbacks_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolManagerImpl);
};

}  // namespace

// static
void MediaTransferProtocolManager::Initialize(
    scoped_refptr<base::MessageLoopProxy> loop_proxy) {
  if (g_media_transfer_protocol_manager) {
    LOG(WARNING) << "MediaTransferProtocolManager was already initialized";
    return;
  }
  g_media_transfer_protocol_manager =
      new MediaTransferProtocolManagerImpl(loop_proxy);
  VLOG(1) << "MediaTransferProtocolManager initialized";
}

// static
void MediaTransferProtocolManager::Shutdown() {
  if (!g_media_transfer_protocol_manager) {
    LOG(WARNING) << "MediaTransferProtocolManager::Shutdown() called with "
                 << "NULL manager";
    return;
  }
  delete g_media_transfer_protocol_manager;
  g_media_transfer_protocol_manager = NULL;
  VLOG(1) << "MediaTransferProtocolManager Shutdown completed";
}

// static
MediaTransferProtocolManager* MediaTransferProtocolManager::GetInstance() {
  return g_media_transfer_protocol_manager;
}

}  // namespace device
