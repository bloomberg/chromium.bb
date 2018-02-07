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
    service_manager::Connector* connector,
    base::FilePath subdirectory)
    : connector_(connector ? connector->Clone() : nullptr),
      subdirectory_(std::move(subdirectory)),
      weak_ptr_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
}

SessionStorageContextMojo::~SessionStorageContextMojo() {}

void SessionStorageContextMojo::OpenSessionStorage(
    const std::string& namespace_id,
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
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

void SessionStorageContextMojo::PurgeMemory() {
  NOTREACHED();
}

void SessionStorageContextMojo::PurgeUnusedWrappersIfNeeded() {
  NOTREACHED();
}

}  // namespace content
