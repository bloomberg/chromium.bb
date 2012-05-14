// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/shared_resources_data_source.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/shared_resources.h"
#include "grit/shared_resources_map.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

int PathToIDR(const std::string& path) {
  int idr = -1;
  if (path == "ui/resources/folder_closed.png") {
    idr = IDR_FOLDER_CLOSED;
  } else if (path == "ui/resources/folder_closed_rtl.png") {
    idr = IDR_FOLDER_CLOSED_RTL;
  } else if (path == "ui/resources/folder_open.png") {
    idr = IDR_FOLDER_OPEN;
  } else if (path == "ui/resources/folder_open_rtl.png") {
    idr = IDR_FOLDER_OPEN_RTL;
  } else if (path == "ui/resources/throbber.png") {
    idr = IDR_THROBBER;
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

    // In touch layout use some alternate CSS rules.
    // Ideally we'd expose a touch-screen media query operator to the web
    // at large, and then just use that for WebUI instead.  crbug.com/123062
    if (idr == IDR_SHARED_CSS_CHROME2 &&
        ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) {
      idr = IDR_SHARED_CSS_CHROME2_TOUCH;
    }
  }

  return idr;
}

}  // namespace

SharedResourcesDataSource::SharedResourcesDataSource()
    : DataSource(chrome::kChromeUIResourcesHost, NULL) {
}

SharedResourcesDataSource::~SharedResourcesDataSource() {
}

void SharedResourcesDataSource::StartDataRequest(const std::string& path,
                                                 bool is_incognito,
                                                 int request_id) {
  int idr = PathToIDR(path);
  DCHECK_NE(-1, idr) << " path: " << path;
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<base::RefCountedStaticMemory> bytes(
      rb.LoadDataResourceBytes(idr));

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
