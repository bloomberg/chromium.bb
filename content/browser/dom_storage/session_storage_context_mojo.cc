// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_context_mojo.h"

#include "content/browser/leveldb_wrapper_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/common/content_features.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

SessionStorageContextMojo::SessionStorageContextMojo(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    service_manager::Connector* connector,
    base::Optional<base::FilePath> partition_directory,
    std::string leveldb_name)
    : connector_(connector ? connector->Clone() : nullptr),
      partition_directory_path_(std::move(partition_directory)),
      leveldb_name_(std::move(leveldb_name)),
      weak_ptr_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
}
SessionStorageContextMojo::~SessionStorageContextMojo() {}

void SessionStorageContextMojo::OpenSessionStorage(
    int process_id,
    const std::string& namespace_id,
    mojom::SessionStorageNamespaceRequest request) {
  // TODO(dmurph): Check the process ID against the origin like so:
  // if (!ChildProcessSecurityPolicy::GetInstance()->CanAccessDataForOrigin(
  //         process_id, origin.GetURL())) {
  //   bindings_.ReportBadMessage("Access denied for sessionStorage request");
  //   return;
  // }
  NOTREACHED();
}

void SessionStorageContextMojo::CreateSessionNamespace(
    const std::string& namespace_id) {
  NOTREACHED();
}

void SessionStorageContextMojo::CloneSessionNamespace(
    const std::string& namespace_id_to_clone,
    const std::string& clone_namespace_id) {
  NOTREACHED();
}

void SessionStorageContextMojo::DeleteSessionNamespace(
    const std::string& namespace_id,
    bool should_persist) {
  NOTREACHED();
}

void SessionStorageContextMojo::Flush() {
  NOTREACHED();
}

void SessionStorageContextMojo::GetStorageUsage(
    GetStorageUsageCallback callback) {
  NOTREACHED();
}
void SessionStorageContextMojo::DeleteStorage(
    const GURL& origin,
    const std::string& persistent_namespace_id) {
  NOTREACHED();
}

void SessionStorageContextMojo::ShutdownAndDelete() {
  delete this;
}

void SessionStorageContextMojo::PurgeMemory() {
  NOTREACHED();
}

void SessionStorageContextMojo::PurgeUnusedWrappersIfNeeded() {
  NOTREACHED();
}

}  // namespace content
