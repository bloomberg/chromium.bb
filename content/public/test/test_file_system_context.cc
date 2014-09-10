// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_system_context.h"

#include "base/memory/scoped_vector.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_file_system_backend.h"
#include "content/public/test/test_file_system_options.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

storage::FileSystemContext* CreateFileSystemContextForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  ScopedVector<storage::FileSystemBackend> additional_providers;
  additional_providers.push_back(new TestFileSystemBackend(
      base::MessageLoopProxy::current().get(), base_path));
  return CreateFileSystemContextWithAdditionalProvidersForTesting(
      quota_manager_proxy, additional_providers.Pass(), base_path);
}

storage::FileSystemContext*
CreateFileSystemContextWithAdditionalProvidersForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<storage::FileSystemBackend> additional_providers,
    const base::FilePath& base_path) {
  return new storage::FileSystemContext(
      base::MessageLoopProxy::current().get(),
      base::MessageLoopProxy::current().get(),
      storage::ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      std::vector<storage::URLRequestAutoMountHandler>(),
      base_path,
      CreateAllowFileAccessOptions());
}

storage::FileSystemContext* CreateFileSystemContextWithAutoMountersForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<storage::FileSystemBackend> additional_providers,
    const std::vector<storage::URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path) {
  return new storage::FileSystemContext(
      base::MessageLoopProxy::current().get(),
      base::MessageLoopProxy::current().get(),
      storage::ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      auto_mounters,
      base_path,
      CreateAllowFileAccessOptions());
}

storage::FileSystemContext* CreateIncognitoFileSystemContextForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  ScopedVector<storage::FileSystemBackend> additional_providers;
  return new storage::FileSystemContext(
      base::MessageLoopProxy::current().get(),
      base::MessageLoopProxy::current().get(),
      storage::ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      std::vector<storage::URLRequestAutoMountHandler>(),
      base_path,
      CreateIncognitoFileSystemOptions());
}

}  // namespace content
