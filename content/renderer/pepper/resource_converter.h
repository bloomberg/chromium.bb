// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H
#define CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"

namespace content {

// This class is responsible for converting V8 vars to Pepper resources.
class CONTENT_EXPORT ResourceConverter {
 public:
  virtual ~ResourceConverter();

 // ShutDown must be called before any vars created by the ResourceConverter
  // are valid. It handles creating any resource hosts that need to be created.
  virtual void ShutDown(const base::Callback<void(bool)>& callback) = 0;
};

class ResourceConverterImpl : public ResourceConverter {
 public:
  ResourceConverterImpl();
  virtual ~ResourceConverterImpl();
  virtual void ShutDown(const base::Callback<void(bool)>& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceConverterImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H
