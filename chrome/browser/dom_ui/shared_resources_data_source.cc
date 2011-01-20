// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/shared_resources_data_source.h"

#include <string>

#include "base/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/shared_resources.h"
#include "grit/shared_resources_map.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

int PathToIDR(const std::string& path) {
  int idr = -1;
  if (path == "app/resources/folder_closed.png") {
    idr = IDR_FOLDER_CLOSED;
  } else if (path == "app/resources/folder_closed_rtl.png") {
    idr = IDR_FOLDER_CLOSED_RTL;
  } else if (path == "app/resources/folder_open.png") {
    idr = IDR_FOLDER_OPEN;
  } else if (path == "app/resources/folder_open_rtl.png") {
    idr = IDR_FOLDER_OPEN_RTL;
  } else {
    // The name of the files in the grd list are prefixed with the following
    // directory:
    std::string key("shared/");
    key += path;

    for (size_t i = 0; i < kSharedResourcesSize; ++i) {
      if (kSharedResources[i].name == key) {
        idr = kSharedResources[i].value;
        break;
      }
    }
  }

  return idr;
}

}  // namespace

// static
void SharedResourcesDataSource::Register() {
  SharedResourcesDataSource* source = new SharedResourcesDataSource();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(source)));
}

SharedResourcesDataSource::SharedResourcesDataSource()
    : DataSource(chrome::kChromeUIResourcesHost, NULL) {
}

SharedResourcesDataSource::~SharedResourcesDataSource() {
}

void SharedResourcesDataSource::StartDataRequest(const std::string& path,
                                                 bool is_off_the_record,
                                                 int request_id) {
  int idr = PathToIDR(path);
  DCHECK_NE(-1, idr);
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<RefCountedStaticMemory> bytes(rb.LoadDataResourceBytes(idr));
  SendResponse(request_id, bytes);
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
