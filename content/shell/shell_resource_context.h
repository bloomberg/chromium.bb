// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_RESOURCE_CONTEXT_H_
#define CONTENT_SHELL_SHELL_RESOURCE_CONTEXT_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/resource_context.h"

class ChromeBlobStorageContext;

namespace content {

class ShellURLRequestContextGetter;

class ShellResourceContext : public content::ResourceContext {
 public:
  ShellResourceContext(
      ShellURLRequestContextGetter* getter,
      ChromeBlobStorageContext* blob_storage_context,
      DownloadManager::GetNextIdThunkType next_download_id_thunk);
  virtual ~ShellResourceContext();

 private:
  virtual void EnsureInitialized() const OVERRIDE;

  void InitializeInternal();

  scoped_refptr<ShellURLRequestContextGetter> getter_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  DownloadManager::GetNextIdThunkType next_download_id_thunk_;

  DISALLOW_COPY_AND_ASSIGN(ShellResourceContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_RESOURCE_CONTEXT_H_
