// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_system_context.h"

#include "base/memory/scoped_vector.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_file_system_backend.h"
#include "content/public/test/test_file_system_options.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace content {

fileapi::FileSystemContext* CreateFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  ScopedVector<fileapi::FileSystemBackend> additional_providers;
  additional_providers.push_back(new TestFileSystemBackend(
      base::MessageLoopProxy::current().get(), base_path));
  return CreateFileSystemContextWithAdditionalProvidersForTesting(
      quota_manager_proxy, additional_providers.Pass(), base_path);
}

fileapi::FileSystemContext*
CreateFileSystemContextWithAdditionalProvidersForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<fileapi::FileSystemBackend> additional_providers,
    const base::FilePath& base_path) {
  return new fileapi::FileSystemContext(
      base::MessageLoopProxy::current().get(),
      base::MessageLoopProxy::current().get(),
      fileapi::ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      std::vector<fileapi::URLRequestAutoMountHandler>(),
      base_path,
      CreateAllowFileAccessOptions());
}

fileapi::FileSystemContext*
CreateFileSystemContextWithAutoMountersForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<fileapi::FileSystemBackend> additional_providers,
    const std::vector<fileapi::URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path) {
  return new fileapi::FileSystemContext(
      base::MessageLoopProxy::current().get(),
      base::MessageLoopProxy::current().get(),
      fileapi::ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      auto_mounters,
      base_path,
      CreateAllowFileAccessOptions());
}

fileapi::FileSystemContext* CreateIncognitoFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  ScopedVector<fileapi::FileSystemBackend> additional_providers;
  return new fileapi::FileSystemContext(
      base::MessageLoopProxy::current().get(),
      base::MessageLoopProxy::current().get(),
      fileapi::ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      std::vector<fileapi::URLRequestAutoMountHandler>(),
      base_path,
      CreateIncognitoFileSystemOptions());
}

}  // namespace content
