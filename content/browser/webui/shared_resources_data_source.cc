// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/mime_util.h"
#include "ui/base/layout.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

namespace content {

namespace {

const char kAppImagesPath[] = "images/apps/";
const char kAppImagesPath2x[] = "images/2x/apps/";

const char kReplacement[] = "../../resources/default_100_percent/common/";
const char kReplacement2x[] = "../../resources/default_200_percent/common/";

// This entire method is a hack introduced to be able to handle apps images
// that exist in the ui/resources directory. From JS/CSS, we still load the
// image as if it were chrome://resources/images/apps/myappimage.png, if that
// path doesn't exist, we check to see if it that image exists in the relative
// path to ui/resources instead.
// TODO(rkc): Once we have a separate source for apps, remove this code.
bool AppsRelativePathMatch(const std::string& path,
                           const std::string& compareto) {
  if (StartsWithASCII(path, kAppImagesPath, false)) {
    if (compareto ==
        (kReplacement + path.substr(arraysize(kAppImagesPath) - 1)))
      return true;
  } else if (StartsWithASCII(path, kAppImagesPath2x, false)) {
    if (compareto ==
        (kReplacement2x + path.substr(arraysize(kAppImagesPath2x) - 1)))
      return true;
  }
  return false;
}

int PathToIDR(const std::string& path) {
  int idr = -1;
  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    if ((path == kWebuiResources[i].name) ||
        AppsRelativePathMatch(path, kWebuiResources[i].name)) {
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

std::string SharedResourcesDataSource::GetSource() const {
  return kChromeUIResourcesHost;
}

void SharedResourcesDataSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const URLDataSource::GotDataCallback& callback) {
  int idr = PathToIDR(path);
  DCHECK_NE(-1, idr) << " path: " << path;
  scoped_refptr<base::RefCountedMemory> bytes;

  ContentClient* content_client = GetContentClient();

  // TODO(dbeam): there's some comments in content/DEPS about disallowing
  // grd-related code. Does using this IDR_* go against that spirit?
  if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS) {
    std::vector<std::string> placeholders;
    placeholders.push_back(webui::GetTextDirection());  // $1
    placeholders.push_back(webui::GetFontFamily());  // $2
    placeholders.push_back(webui::GetFontSize());  // $3

    const std::string& chrome_shared =
        content_client->GetDataResource(idr, ui::SCALE_FACTOR_NONE).as_string();
    std::string replaced =
        ReplaceStringPlaceholders(chrome_shared, placeholders, nullptr);
    bytes = base::RefCountedString::TakeString(&replaced);
  } else {
    bytes = content_client->GetDataResourceBytes(idr);
  }

  callback.Run(bytes.get());
}

std::string SharedResourcesDataSource::GetMimeType(
    const std::string& path) const {
  // Requests should not block on the disk!  On POSIX this goes to disk.
  // http://code.google.com/p/chromium/issues/detail?id=59849

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string mime_type;
  net::GetMimeTypeFromFile(base::FilePath().AppendASCII(path), &mime_type);
  return mime_type;
}

std::string
SharedResourcesDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) const {
  // For now we give access only for "chrome://*" origins.
  // According to CORS spec, Access-Control-Allow-Origin header doesn't support
  // wildcards, so we need to set its value explicitly by passing the |origin|
  // back.
  std::string allowed_origin_prefix = kChromeUIScheme;
  allowed_origin_prefix += "://";
  if (origin.find(allowed_origin_prefix) != 0)
    return "none";
  return origin;
}

}  // namespace content
