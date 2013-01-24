// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "grit/webui_resources_map.h"
#include "net/base/mime_util.h"

namespace {

int PathToIDR(const std::string& path) {
  int idr = -1;
  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    if (kWebuiResources[i].name == path) {
      idr = kWebuiResources[i].value;
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
  scoped_refptr<base::RefCountedStaticMemory> bytes(
      content::GetContentClient()->GetDataResourceBytes(idr));

  callback.Run(bytes);
}

std::string SharedResourcesDataSource::GetMimeType(
    const std::string& path) const {
  // Requests should not block on the disk!  On POSIX this goes to disk.
  // http://code.google.com/p/chromium/issues/detail?id=59849

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string mime_type;
  net::GetMimeTypeFromFile(FilePath().AppendASCII(path), &mime_type);
  return mime_type;
}
