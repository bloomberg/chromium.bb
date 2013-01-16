// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/shared_resources_data_source.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/shared_resources.h"
#include "grit/shared_resources_map.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

int PathToIDR(const std::string& path) {
  // The name of the files in the grd list are prefixed with the following
  // directory:
  std::string key("shared/");
  key += path;

  int idr = -1;
  for (size_t i = 0; i < kSharedResourcesSize; ++i) {
    if (kSharedResources[i].name == key) {
      idr = kSharedResources[i].value;
      break;
    }
  }

  return idr;
}

}  // namespace

SharedResourcesDataSource::SharedResourcesDataSource() {
}

SharedResourcesDataSource::~SharedResourcesDataSource() {
}

std::string SharedResourcesDataSource::GetSource() {
  return chrome::kChromeUIResourcesHost;
}

void SharedResourcesDataSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  int idr = PathToIDR(path);
  DCHECK_NE(-1, idr) << " path: " << path;
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<base::RefCountedStaticMemory> bytes(
      rb.LoadDataResourceBytes(idr));

  callback.Run(bytes);
}

std::string SharedResourcesDataSource::GetMimeType(
    const std::string& path) const {
  // Requests should not block on the disk!  On Windows this goes to the
  // registry.
  //   http://code.google.com/p/chromium/issues/detail?id=59849
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::string mime_type;
  net::GetMimeTypeFromFile(FilePath().AppendASCII(path), &mime_type);
  return mime_type;
}
