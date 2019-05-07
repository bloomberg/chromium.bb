// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_manager_impl.h"

#include "base/task/post_task.h"
#include "content/browser/native_file_system/file_system_chooser.h"
#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"

using blink::mojom::NativeFileSystemError;

namespace content {

NativeFileSystemManagerImpl::NativeFileSystemManagerImpl(
    scoped_refptr<storage::FileSystemContext> context,
    scoped_refptr<ChromeBlobStorageContext> blob_context)
    : context_(std::move(context)), blob_context_(std::move(blob_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_context_);
}

NativeFileSystemManagerImpl::~NativeFileSystemManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void NativeFileSystemManagerImpl::BindRequest(
    const url::Origin& origin,
    int process_id,
    int frame_id,
    blink::mojom::NativeFileSystemManagerRequest request) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI));

  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bindings_.AddBinding(this, std::move(request),
                       BindingContext(origin, process_id, frame_id));
}

void NativeFileSystemManagerImpl::GetSandboxedFileSystem(
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  url::Origin origin = bindings_.dispatch_context().origin;

  context()->OpenFileSystem(
      origin.GetURL(), storage::kFileSystemTypeTemporary,
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeFileSystemManagerImpl::ChooseEntries(
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all,
    ChooseEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const BindingContext& context = bindings_.dispatch_context();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &FileSystemChooser::CreateAndShow, context.process_id,
          context.frame_id, type, std::move(accepts), include_accepts_all,
          base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                         weak_factory_.GetWeakPtr(), type, context.origin,
                         std::move(callback)),
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})));
}

blink::mojom::NativeFileSystemFileHandlePtr
NativeFileSystemManagerImpl::CreateFileHandle(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(url.is_valid());
  blink::mojom::NativeFileSystemFileHandlePtr result;
  file_bindings_.AddBinding(
      std::make_unique<NativeFileSystemFileHandleImpl>(this, url),
      mojo::MakeRequest(&result));
  return result;
}

blink::mojom::NativeFileSystemDirectoryHandlePtr
NativeFileSystemManagerImpl::CreateDirectoryHandle(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(url.is_valid());

  blink::mojom::NativeFileSystemDirectoryHandlePtr result;
  directory_bindings_.AddBinding(
      std::make_unique<NativeFileSystemDirectoryHandleImpl>(this, url),
      mojo::MakeRequest(&result));
  return result;
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemFileHandleImpl& file,
    blink::mojom::NativeFileSystemTransferTokenRequest request) {
  return CreateTransferTokenImpl(file.url(),
                                 /*is_directory=*/false, std::move(request));
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemDirectoryHandleImpl& directory,
    blink::mojom::NativeFileSystemTransferTokenRequest request) {
  return CreateTransferTokenImpl(directory.url(),
                                 /*is_directory=*/true, std::move(request));
}

void NativeFileSystemManagerImpl::ResolveTransferToken(
    blink::mojom::NativeFileSystemTransferTokenPtr token,
    ResolvedTokenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto* raw_token = token.get();
  raw_token->GetInternalID(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&NativeFileSystemManagerImpl::DoResolveTransferToken,
                     weak_factory_.GetWeakPtr(), std::move(token),
                     std::move(callback)),
      base::UnguessableToken()));
}

storage::FileSystemOperationRunner*
NativeFileSystemManagerImpl::operation_runner() {
  if (!operation_runner_)
    operation_runner_ = context()->CreateFileSystemOperationRunner();
  return operation_runner_.get();
}

void NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem(
    GetSandboxedFileSystemCallback callback,
    const GURL& root,
    const std::string& filesystem_name,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(result), nullptr);
    return;
  }

  std::move(callback).Run(NativeFileSystemError::New(base::File::FILE_OK),
                          CreateDirectoryHandle(context()->CrackURL(root)));
}

void NativeFileSystemManagerImpl::DidChooseEntries(
    blink::mojom::ChooseFileSystemEntryType type,
    const url::Origin& origin,
    ChooseEntriesCallback callback,
    blink::mojom::NativeFileSystemErrorPtr result,
    std::vector<FileSystemChooser::IsolatedFileSystemEntry> entries) {
  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (result->error_code != base::File::FILE_OK) {
    std::move(callback).Run(std::move(result), std::move(result_entries));
    return;
  }
  result_entries.reserve(entries.size());
  for (const auto& entry : entries) {
    auto url = context()->CreateCrackedFileSystemURL(
        origin.GetURL(), storage::kFileSystemTypeIsolated,
        entry.isolated_file_path);
    std::string name = storage::FilePathToString(
        storage::VirtualPath::BaseName(entry.isolated_file_path));
    // TODO(https://crbug.com/955185): Pass file system ID to the handle
    // implementations so they can properly ref and unref the filesystem.
    // Right now the isolated file system will just be leaked and never freed.
    if (type == blink::mojom::ChooseFileSystemEntryType::kOpenDirectory) {
      result_entries.push_back(blink::mojom::NativeFileSystemEntry::New(
          blink::mojom::NativeFileSystemHandle::NewDirectory(
              CreateDirectoryHandle(url).PassInterface()),
          name));
    } else {
      result_entries.push_back(blink::mojom::NativeFileSystemEntry::New(
          blink::mojom::NativeFileSystemHandle::NewFile(
              CreateFileHandle(url).PassInterface()),
          name));
    }
  }
  std::move(callback).Run(std::move(result), std::move(result_entries));
}

void NativeFileSystemManagerImpl::CreateTransferTokenImpl(
    const storage::FileSystemURL& url,
    bool is_directory,
    blink::mojom::NativeFileSystemTransferTokenRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto token_impl = std::make_unique<NativeFileSystemTransferTokenImpl>(
      url, is_directory
               ? NativeFileSystemTransferTokenImpl::HandleType::kDirectory
               : NativeFileSystemTransferTokenImpl::HandleType::kFile);
  auto token = token_impl->token();
  blink::mojom::NativeFileSystemTransferTokenPtr result;
  auto emplace_result = transfer_tokens_.emplace(
      std::piecewise_construct, std::forward_as_tuple(token),
      std::forward_as_tuple(std::move(token_impl), std::move(request)));
  DCHECK(emplace_result.second);
  emplace_result.first->second.set_connection_error_handler(base::BindOnce(
      &NativeFileSystemManagerImpl::TransferTokenConnectionErrorHandler,
      base::Unretained(this), token));
}

void NativeFileSystemManagerImpl::TransferTokenConnectionErrorHandler(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  size_t count_removed = transfer_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void NativeFileSystemManagerImpl::DoResolveTransferToken(
    blink::mojom::NativeFileSystemTransferTokenPtr,
    ResolvedTokenCallback callback,
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = transfer_tokens_.find(token);
  if (it == transfer_tokens_.end()) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(
        static_cast<NativeFileSystemTransferTokenImpl*>(it->second.impl()));
  }
}

}  // namespace content