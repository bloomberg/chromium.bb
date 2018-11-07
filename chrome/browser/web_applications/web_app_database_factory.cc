// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/model_impl/model_type_store_service_impl.h"

namespace web_app {

constexpr base::FilePath::CharType kWebAppsFolderName[] =
    FILE_PATH_LITERAL("WebApps");

WebAppDatabaseFactory::WebAppDatabaseFactory(Profile* profile)
    : model_type_store_service_(
          std::make_unique<syncer::ModelTypeStoreServiceImpl>(
              profile->GetPath().Append(base::FilePath(kWebAppsFolderName)))) {}

WebAppDatabaseFactory::~WebAppDatabaseFactory() {}

syncer::OnceModelTypeStoreFactory WebAppDatabaseFactory::GetStoreFactory() {
  return model_type_store_service_->GetStoreFactory();
}

}  // namespace web_app
