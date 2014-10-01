// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_DESCRIPTOR_INFO_IMPL_H_
#define CONTENT_PUBLIC_BROWSER_FILE_DESCRIPTOR_INFO_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_descriptor_info.h"

namespace content {

class FileDescriptorInfoImpl : public FileDescriptorInfo {
 public:
  CONTENT_EXPORT static scoped_ptr<FileDescriptorInfo> Create();

  virtual ~FileDescriptorInfoImpl();
  virtual void Share(int id, base::PlatformFile fd) OVERRIDE;
  virtual void Transfer(int id, base::ScopedFD fd) OVERRIDE;
  virtual const base::FileHandleMappingVector& GetMapping() const OVERRIDE;
  virtual base::FileHandleMappingVector GetMappingWithIDAdjustment(
      int delta) const OVERRIDE;
  virtual base::PlatformFile GetFDAt(size_t i) const OVERRIDE;
  virtual int GetIDAt(size_t i) const OVERRIDE;
  virtual size_t GetMappingSize() const OVERRIDE;

 private:
  FileDescriptorInfoImpl();

  void AddToMapping(int id, base::PlatformFile fd);
  bool HasID(int id) const;
  base::FileHandleMappingVector mapping_;
  ScopedVector<base::ScopedFD> owned_descriptors_;
};
}

#endif  // CONTENT_PUBLIC_BROWSER_FILE_DESCRIPTOR_INFO_IMPL_H_
