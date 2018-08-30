// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fileapi/file_system_dispatcher.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/worker_thread.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_type_converters.h"

namespace content {

class FileSystemDispatcher::CallbackDispatcher {
 public:
  static std::unique_ptr<CallbackDispatcher> Create(
      const StatusCallback& callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->status_callback_ = callback;
    dispatcher->error_callback_ = callback;
    return dispatcher;
  }
  static std::unique_ptr<CallbackDispatcher> Create(
      const MetadataCallback& callback,
      const StatusCallback& error_callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->metadata_callback_ = callback;
    dispatcher->error_callback_ = error_callback;
    return dispatcher;
  }
  static std::unique_ptr<CallbackDispatcher> Create(
      const CreateSnapshotFileCallback& callback,
      const StatusCallback& error_callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->snapshot_callback_ = callback;
    dispatcher->error_callback_ = error_callback;
    return dispatcher;
  }
  static std::unique_ptr<CallbackDispatcher> Create(
      const ReadDirectoryCallback& callback,
      const StatusCallback& error_callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->directory_callback_ = callback;
    dispatcher->error_callback_ = error_callback;
    return dispatcher;
  }
  static std::unique_ptr<CallbackDispatcher> Create(
      const OpenFileSystemCallback& callback,
      const StatusCallback& error_callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->filesystem_callback_ = callback;
    dispatcher->error_callback_ = error_callback;
    return dispatcher;
  }
  static std::unique_ptr<CallbackDispatcher> Create(
      const ResolveURLCallback& callback,
      const StatusCallback& error_callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->resolve_callback_ = callback;
    dispatcher->error_callback_ = error_callback;
    return dispatcher;
  }
  static std::unique_ptr<CallbackDispatcher> Create(
      const WriteCallback& callback,
      const StatusCallback& error_callback) {
    auto dispatcher = base::WrapUnique(new CallbackDispatcher);
    dispatcher->write_callback_ = callback;
    dispatcher->error_callback_ = error_callback;
    return dispatcher;
  }

  ~CallbackDispatcher() {}

  void DidSucceed() { status_callback_.Run(base::File::FILE_OK); }

  void DidFail(base::File::Error error_code) {
    error_callback_.Run(error_code);
  }

  void DidReadMetadata(const base::File::Info& file_info) {
    metadata_callback_.Run(file_info);
  }

  void DidCreateSnapshotFile(
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      base::Optional<blink::mojom::ReceivedSnapshotListenerPtr> opt_listener,
      int request_id) {
    snapshot_callback_.Run(file_info, platform_path, std::move(opt_listener),
                           request_id);
  }

  void DidReadDirectory(
      const std::vector<filesystem::mojom::DirectoryEntry>& entries,
      bool has_more) {
    directory_callback_.Run(entries, has_more);
  }

  void DidOpenFileSystem(const std::string& name, const GURL& root) {
    filesystem_callback_.Run(name, root);
  }

  void DidResolveURL(const storage::FileSystemInfo& info,
                     const base::FilePath& file_path,
                     bool is_directory) {
    resolve_callback_.Run(info, file_path, is_directory);
  }

  void DidWrite(int64_t bytes, bool complete) {
    write_callback_.Run(bytes, complete);
  }

 private:
  CallbackDispatcher() {}

  StatusCallback status_callback_;
  MetadataCallback metadata_callback_;
  CreateSnapshotFileCallback snapshot_callback_;
  ReadDirectoryCallback directory_callback_;
  OpenFileSystemCallback filesystem_callback_;
  ResolveURLCallback resolve_callback_;
  WriteCallback write_callback_;

  StatusCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackDispatcher);
};

class FileSystemDispatcher::FileSystemOperationListenerImpl
    : public blink::mojom::FileSystemOperationListener {
 public:
  FileSystemOperationListenerImpl(int request_id,
                                  FileSystemDispatcher* dispatcher)
      : request_id_(request_id), dispatcher_(dispatcher) {}

 private:
  // blink::mojom::FileSystemOperationListener
  void ResultsRetrieved(
      std::vector<filesystem::mojom::DirectoryEntryPtr> entries,
      bool has_more) override;
  void ErrorOccurred(base::File::Error error_code) override;
  void DidWrite(int64_t byte_count, bool complete) override;

  const int request_id_;
  FileSystemDispatcher* const dispatcher_;
};

FileSystemDispatcher::FileSystemDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(std::move(main_thread_task_runner)) {}

FileSystemDispatcher::~FileSystemDispatcher() {
  // Make sure we fire all the remaining callbacks.
  for (base::IDMap<std::unique_ptr<CallbackDispatcher>>::iterator iter(
           &dispatchers_);
       !iter.IsAtEnd(); iter.Advance()) {
    int request_id = iter.GetCurrentKey();
    CallbackDispatcher* dispatcher = iter.GetCurrentValue();
    DCHECK(dispatcher);
    dispatcher->DidFail(base::File::FILE_ERROR_ABORT);
    dispatchers_.Remove(request_id);
  }
}

blink::mojom::FileSystemManager& FileSystemDispatcher::GetFileSystemManager() {
  auto BindInterfaceOnMainThread =
      [](blink::mojom::FileSystemManagerRequest request) {
        DCHECK(ChildThreadImpl::current());
        ChildThreadImpl::current()->GetConnector()->BindInterface(
            mojom::kBrowserServiceName, std::move(request));
      };
  if (!file_system_manager_ptr_) {
    if (WorkerThread::GetCurrentId()) {
      blink::mojom::FileSystemManagerRequest request =
          mojo::MakeRequest(&file_system_manager_ptr_);
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(BindInterfaceOnMainThread, std::move(request)));
    } else {
      BindInterfaceOnMainThread(mojo::MakeRequest(&file_system_manager_ptr_));
    }
  }
  return *file_system_manager_ptr_;
}

void FileSystemDispatcher::OpenFileSystem(
    const GURL& origin_url,
    storage::FileSystemType type,
    const OpenFileSystemCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  GetFileSystemManager().Open(
      origin_url, mojo::ConvertTo<blink::mojom::FileSystemType>(type),
      base::BindOnce(&FileSystemDispatcher::DidOpenFileSystem,
                     base::Unretained(this), request_id));
}

void FileSystemDispatcher::OpenFileSystemSync(
    const GURL& origin_url,
    storage::FileSystemType type,
    const OpenFileSystemCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  std::string name;
  GURL root_url;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Open(
      origin_url, mojo::ConvertTo<blink::mojom::FileSystemType>(type), &name,
      &root_url, &error_code);
  DidOpenFileSystem(request_id, std::move(name), root_url, error_code);
}

void FileSystemDispatcher::ResolveURL(
    const GURL& filesystem_url,
    const ResolveURLCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  GetFileSystemManager().ResolveURL(
      filesystem_url, base::BindOnce(&FileSystemDispatcher::DidResolveURL,
                                     base::Unretained(this), request_id));
}

void FileSystemDispatcher::ResolveURLSync(
    const GURL& filesystem_url,
    const ResolveURLCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  blink::mojom::FileSystemInfoPtr info;
  base::FilePath file_path;
  bool is_directory;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ResolveURL(filesystem_url, &info, &file_path,
                                    &is_directory, &error_code);
  DidResolveURL(request_id, std::move(info), std::move(file_path), is_directory,
                error_code);
}

void FileSystemDispatcher::Move(const GURL& src_path,
                                const GURL& dest_path,
                                const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().Move(
      src_path, dest_path,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::MoveSync(const GURL& src_path,
                                    const GURL& dest_path,
                                    const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Move(src_path, dest_path, &error_code);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::Copy(const GURL& src_path,
                                const GURL& dest_path,
                                const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().Copy(
      src_path, dest_path,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::CopySync(const GURL& src_path,
                                    const GURL& dest_path,
                                    const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Copy(src_path, dest_path, &error_code);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::Remove(const GURL& path,
                                  bool recursive,
                                  const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().Remove(
      path, recursive,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::RemoveSync(const GURL& path,
                                      bool recursive,
                                      const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Remove(path, recursive, &error_code);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::ReadMetadata(
    const GURL& path,
    const MetadataCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  GetFileSystemManager().ReadMetadata(
      path, base::BindOnce(&FileSystemDispatcher::DidReadMetadata,
                           base::Unretained(this), request_id));
}

void FileSystemDispatcher::ReadMetadataSync(
    const GURL& path,
    const MetadataCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  base::File::Info file_info;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ReadMetadata(path, &file_info, &error_code);
  DidReadMetadata(request_id, std::move(file_info), error_code);
}

void FileSystemDispatcher::CreateFile(const GURL& path,
                                      bool exclusive,
                                      const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().Create(
      path, exclusive, /*is_directory=*/false, /*is_recursive=*/false,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::CreateFileSync(const GURL& path,
                                          bool exclusive,
                                          const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Create(path, exclusive, /*is_directory=*/false,
                                /*is_recursive=*/false, &error_code);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::CreateDirectory(const GURL& path,
                                           bool exclusive,
                                           bool recursive,
                                           const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().Create(
      path, exclusive, true, recursive,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::CreateDirectorySync(const GURL& path,
                                               bool exclusive,
                                               bool recursive,
                                               const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Create(path, exclusive, true, recursive, &error_code);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::Exists(const GURL& path,
                                  bool is_directory,
                                  const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().Exists(
      path, is_directory,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::ExistsSync(const GURL& path,
                                      bool is_directory,
                                      const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Exists(path, is_directory, &error_code);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::ReadDirectory(
    const GURL& path,
    const ReadDirectoryCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  blink::mojom::FileSystemOperationListenerPtr ptr;
  blink::mojom::FileSystemOperationListenerRequest request =
      mojo::MakeRequest(&ptr);
  op_listeners_.AddBinding(
      std::make_unique<FileSystemOperationListenerImpl>(request_id, this),
      std::move(request));
  GetFileSystemManager().ReadDirectory(path, std::move(ptr));
}

void FileSystemDispatcher::ReadDirectorySync(
    const GURL& path,
    const ReadDirectoryCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  std::vector<filesystem::mojom::DirectoryEntryPtr> entries;
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ReadDirectorySync(path, &entries, &result);
  if (result == base::File::FILE_OK)
    DidReadDirectory(request_id, std::move(entries), /*has_more=*/false);
  else
    DidFail(request_id, result);
}

void FileSystemDispatcher::Truncate(const GURL& path,
                                    int64_t offset,
                                    int* request_id_out,
                                    const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  blink::mojom::FileSystemCancellableOperationPtr op_ptr;
  blink::mojom::FileSystemCancellableOperationRequest op_request =
      mojo::MakeRequest(&op_ptr);
  op_ptr.set_connection_error_handler(
      base::BindOnce(&FileSystemDispatcher::RemoveOperationPtr,
                     base::Unretained(this), request_id));
  cancellable_operations_[request_id] = std::move(op_ptr);
  GetFileSystemManager().Truncate(
      path, offset, std::move(op_request),
      base::BindOnce(&FileSystemDispatcher::DidTruncate, base::Unretained(this),
                     request_id));

  if (request_id_out)
    *request_id_out = request_id;
}

void FileSystemDispatcher::TruncateSync(const GURL& path,
                                        int64_t offset,
                                        const StatusCallback& callback) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().TruncateSync(path, offset, &error_code);
  std::move(callback).Run(error_code);
}

void FileSystemDispatcher::Write(const GURL& path,
                                 const std::string& blob_id,
                                 int64_t offset,
                                 int* request_id_out,
                                 const WriteCallback& success_callback,
                                 const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));

  blink::mojom::FileSystemCancellableOperationPtr op_ptr;
  blink::mojom::FileSystemCancellableOperationRequest op_request =
      mojo::MakeRequest(&op_ptr);
  op_ptr.set_connection_error_handler(
      base::BindOnce(&FileSystemDispatcher::RemoveOperationPtr,
                     base::Unretained(this), request_id));
  cancellable_operations_[request_id] = std::move(op_ptr);

  blink::mojom::FileSystemOperationListenerPtr listener_ptr;
  blink::mojom::FileSystemOperationListenerRequest request =
      mojo::MakeRequest(&listener_ptr);
  op_listeners_.AddBinding(
      std::make_unique<FileSystemOperationListenerImpl>(request_id, this),
      std::move(request));

  GetFileSystemManager().Write(path, blob_id, offset, std::move(op_request),
                               std::move(listener_ptr));

  if (request_id_out)
    *request_id_out = request_id;
}

void FileSystemDispatcher::WriteSync(const GURL& path,
                                     const std::string& blob_id,
                                     int64_t offset,
                                     const WriteCallback& success_callback,
                                     const StatusCallback& error_callback) {
  int64_t byte_count;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().WriteSync(path, blob_id, offset, &byte_count,
                                   &error_code);
  if (error_code == base::File::FILE_OK)
    std::move(success_callback).Run(byte_count, /*complete=*/true);
  else
    std::move(error_callback).Run(error_code);
}

void FileSystemDispatcher::Cancel(int request_id_to_cancel,
                                  const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  if (cancellable_operations_.find(request_id_to_cancel) ==
      cancellable_operations_.end()) {
    DidFail(request_id, base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  cancellable_operations_[request_id_to_cancel]->Cancel(
      base::BindOnce(&FileSystemDispatcher::DidCancel, base::Unretained(this),
                     request_id, request_id_to_cancel));
}

void FileSystemDispatcher::TouchFile(const GURL& path,
                                     const base::Time& last_access_time,
                                     const base::Time& last_modified_time,
                                     const StatusCallback& callback) {
  int request_id = dispatchers_.Add(CallbackDispatcher::Create(callback));
  GetFileSystemManager().TouchFile(
      path, last_access_time, last_modified_time,
      base::BindOnce(&FileSystemDispatcher::DidFinish, base::Unretained(this),
                     request_id));
}

void FileSystemDispatcher::CreateSnapshotFile(
    const GURL& file_path,
    const CreateSnapshotFileCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  GetFileSystemManager().CreateSnapshotFile(
      file_path, base::BindOnce(&FileSystemDispatcher::DidCreateSnapshotFile,
                                base::Unretained(this), request_id));
}

void FileSystemDispatcher::CreateSnapshotFileSync(
    const GURL& file_path,
    const CreateSnapshotFileCallback& success_callback,
    const StatusCallback& error_callback) {
  int request_id = dispatchers_.Add(
      CallbackDispatcher::Create(success_callback, error_callback));
  base::File::Info file_info;
  base::FilePath platform_path;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  blink::mojom::ReceivedSnapshotListenerPtr listener;
  GetFileSystemManager().CreateSnapshotFile(
      file_path, &file_info, &platform_path, &error_code, &listener);
  DidCreateSnapshotFile(request_id, std::move(file_info),
                        std::move(platform_path), error_code,
                        std::move(listener));
}

void FileSystemDispatcher::CreateFileWriter(
    const GURL& file_path,
    std::unique_ptr<CreateFileWriterCallbacks> callbacks) {
  GetFileSystemManager().CreateWriter(
      file_path,
      base::BindOnce(
          [](std::unique_ptr<CreateFileWriterCallbacks> callbacks,
             base::File::Error result, blink::mojom::FileWriterPtr writer) {
            if (result != base::File::FILE_OK) {
              callbacks->OnError(result);
            } else {
              callbacks->OnSuccess(writer.PassInterface().PassHandle());
            }
          },
          std::move(callbacks)));
}

void FileSystemDispatcher::ChooseEntry(
    int render_frame_id,
    std::unique_ptr<ChooseEntryCallbacks> callbacks) {
  GetFileSystemManager().ChooseEntry(
      render_frame_id,
      base::BindOnce(
          [](std::unique_ptr<ChooseEntryCallbacks> callbacks,
             base::File::Error result,
             std::vector<blink::mojom::FileSystemEntryPtr> entries) {
            if (result != base::File::FILE_OK) {
              callbacks->OnError(result);
            } else {
              blink::WebVector<blink::WebFileSystem::FileSystemEntry>
                  web_entries(entries.size());
              for (size_t i = 0; i < entries.size(); ++i) {
                web_entries[i].file_system_id =
                    blink::WebString::FromASCII(entries[i]->file_system_id);
                web_entries[i].base_name =
                    blink::WebString::FromASCII(entries[i]->base_name);
              }
              callbacks->OnSuccess(std::move(web_entries));
            }
          },
          std::move(callbacks)));
}

void FileSystemDispatcher::DidOpenFileSystem(int request_id,
                                             const std::string& name,
                                             const GURL& root,
                                             base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
    DCHECK(dispatcher);
    dispatcher->DidOpenFileSystem(name, root);
    dispatchers_.Remove(request_id);
  } else {
    DidFail(request_id, error_code);
  }
}

void FileSystemDispatcher::DidResolveURL(int request_id,
                                         blink::mojom::FileSystemInfoPtr info,
                                         const base::FilePath& file_path,
                                         bool is_directory,
                                         base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    DCHECK(info->root_url.is_valid());
    CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
    DCHECK(dispatcher);
    dispatcher->DidResolveURL(mojo::ConvertTo<storage::FileSystemInfo>(info),
                              file_path, is_directory);
    dispatchers_.Remove(request_id);
  } else {
    DidFail(request_id, error_code);
  }
}

void FileSystemDispatcher::DidFinish(int request_id,
                                     base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
    DCHECK(dispatcher);
    dispatcher->DidSucceed();
    dispatchers_.Remove(request_id);
  } else {
    DidFail(request_id, error_code);
  }
}

void FileSystemDispatcher::DidReadMetadata(int request_id,
                                           const base::File::Info& file_info,
                                           base::File::Error error_code) {
  if (error_code == base::File::FILE_OK) {
    CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
    DCHECK(dispatcher);
    dispatcher->DidReadMetadata(file_info);
    dispatchers_.Remove(request_id);
  } else {
    DidFail(request_id, error_code);
  }
}

void FileSystemDispatcher::DidCreateSnapshotFile(
    int request_id,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    base::File::Error error_code,
    blink::mojom::ReceivedSnapshotListenerPtr listener) {
  base::Optional<blink::mojom::ReceivedSnapshotListenerPtr> opt_listener;
  if (listener)
    opt_listener = std::move(listener);
  if (error_code == base::File::FILE_OK) {
    CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
    DCHECK(dispatcher);
    dispatcher->DidCreateSnapshotFile(file_info, platform_path,
                                      std::move(opt_listener), request_id);
    dispatchers_.Remove(request_id);
  } else {
    DidFail(request_id, error_code);
  }
}

void FileSystemDispatcher::DidReadDirectory(
    int request_id,
    std::vector<filesystem::mojom::DirectoryEntryPtr> entries,
    bool has_more) {
  CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  std::vector<filesystem::mojom::DirectoryEntry> entries_copy;
  for (const auto& entry : entries) {
    entries_copy.push_back(*entry);
  }
  dispatcher->DidReadDirectory(std::move(entries_copy), has_more);
  if (!has_more)
    dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::DidFail(int request_id,
                                   base::File::Error error_code) {
  CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidFail(error_code);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::DidWrite(int request_id,
                                    int64_t bytes,
                                    bool complete) {
  CallbackDispatcher* dispatcher = dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidWrite(bytes, complete);
  if (complete) {
    dispatchers_.Remove(request_id);
    RemoveOperationPtr(request_id);
  }
}

void FileSystemDispatcher::DidTruncate(int request_id,
                                       base::File::Error error_code) {
  // If |error_code| is ABORT, it means the operation was cancelled,
  // so we let DidCancel clean up the interface pointer.
  if (error_code != base::File::FILE_ERROR_ABORT)
    RemoveOperationPtr(request_id);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::DidCancel(int request_id,
                                     int cancelled_request_id,
                                     base::File::Error error_code) {
  if (error_code == base::File::FILE_OK)
    RemoveOperationPtr(cancelled_request_id);
  DidFinish(request_id, error_code);
}

void FileSystemDispatcher::FileSystemOperationListenerImpl::ResultsRetrieved(
    std::vector<filesystem::mojom::DirectoryEntryPtr> entries,
    bool has_more) {
  dispatcher_->DidReadDirectory(request_id_, std::move(entries), has_more);
}

void FileSystemDispatcher::FileSystemOperationListenerImpl::ErrorOccurred(
    base::File::Error error_code) {
  dispatcher_->DidFail(request_id_, error_code);
}

void FileSystemDispatcher::FileSystemOperationListenerImpl::DidWrite(
    int64_t byte_count,
    bool complete) {
  dispatcher_->DidWrite(request_id_, byte_count, complete);
}

}  // namespace content
