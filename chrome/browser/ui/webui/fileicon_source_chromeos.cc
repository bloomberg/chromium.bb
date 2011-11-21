// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/fileicon_source_chromeos.h"

#include <map>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/memory/singleton.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/component_extension_resources.h"
#include "grit/component_extension_resources_map.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

struct IdrBySize {
  int idr_small_;
  int idr_normal_;
  int idr_large_;
};

typedef std::map<std::string, IconLoader::IconSize> QueryIconSizeMap;
typedef std::map<std::string, IdrBySize> ExtensionIconSizeMap;

const char *kIconSize = "iconsize";

const IdrBySize kAudioIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_AUDIO,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_AUDIO,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_AUDIO
};
const IdrBySize kDeviceIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_DEVICE,
  IDR_FILE_MANAGER_IMG_FILETYPE_DEVICE,
  IDR_FILE_MANAGER_IMG_FILETYPE_DEVICE
};
const IdrBySize kDocIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_DOC,
  IDR_FILE_MANAGER_IMG_FILETYPE_DOC,
  IDR_FILE_MANAGER_IMG_FILETYPE_DOC
};
const IdrBySize kFolderIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_FOLDER,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_FOLDER,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_FOLDER
};
const IdrBySize kGenericIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_GENERIC,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_GENERIC,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_GENERIC
};
const IdrBySize kHtmlIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_HTML,
  IDR_FILE_MANAGER_IMG_FILETYPE_HTML,
  IDR_FILE_MANAGER_IMG_FILETYPE_HTML
};
const IdrBySize kImageIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_IMAGE,
  IDR_FILE_MANAGER_IMG_FILETYPE_IMAGE,
  IDR_FILE_MANAGER_IMG_FILETYPE_IMAGE
};
const IdrBySize kPdfIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_PDF,
  IDR_FILE_MANAGER_IMG_FILETYPE_PDF,
  IDR_FILE_MANAGER_IMG_FILETYPE_PDF
};
const IdrBySize kPresentationIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_PRESENTATION,
  IDR_FILE_MANAGER_IMG_FILETYPE_PRESENTATION,
  IDR_FILE_MANAGER_IMG_FILETYPE_PRESENTATION
};
const IdrBySize kSpreadsheetIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_SPREADSHEET,
  IDR_FILE_MANAGER_IMG_FILETYPE_SPREADSHEET,
  IDR_FILE_MANAGER_IMG_FILETYPE_SPREADSHEET
};
const IdrBySize kUnreadableDeviceIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_UNREADABLE_DEVICE,
  IDR_FILE_MANAGER_IMG_FILETYPE_UNREADABLE_DEVICE,
  IDR_FILE_MANAGER_IMG_FILETYPE_UNREADABLE_DEVICE
};
const IdrBySize kTextIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_TEXT,
  IDR_FILE_MANAGER_IMG_FILETYPE_TEXT,
  IDR_FILE_MANAGER_IMG_FILETYPE_TEXT
};
const IdrBySize kVideoIdrs = {
  IDR_FILE_MANAGER_IMG_FILETYPE_VIDEO,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_VIDEO,
  IDR_FILE_MANAGER_IMG_FILETYPE_LARGE_VIDEO
};

QueryIconSizeMap BuildQueryIconSizeMap() {
  QueryIconSizeMap::value_type kQueryIconSizeData[] = {
    std::make_pair("small", IconLoader::SMALL),
    std::make_pair("normal", IconLoader::NORMAL),
    std::make_pair("large", IconLoader::LARGE)
  };

  size_t kQSize = arraysize(kQueryIconSizeData);
  return QueryIconSizeMap(&kQueryIconSizeData[0],
                          &kQueryIconSizeData[kQSize]);
}

// The code below should match translation in
// chrome/browser/resources/file_manager/js/file_manager.js
// chrome/browser/resources/file_manager/css/file_manager.css
// 'audio': /\.(mp3|m4a|oga|ogg|wav)$/i,
// 'html': /\.(html?)$/i,
// 'image': /\.(bmp|gif|jpe?g|ico|png|webp)$/i,
// 'pdf' : /\.(pdf)$/i,
// 'text': /\.(pod|rst|txt|log)$/i,
// 'video': /\.(mov|mp4|m4v|mpe?g4?|ogm|ogv|ogx|webm)$/i

ExtensionIconSizeMap BuildExtensionIdrSizeMap() {
  const ExtensionIconSizeMap::value_type kExtensionIdrBySizeData[] = {
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
    std::make_pair(".m4a", kAudioIdrs),
    std::make_pair(".mp3", kAudioIdrs),
    std::make_pair(".pdf", kPdfIdrs),
    std::make_pair(".3gp", kVideoIdrs),
    std::make_pair(".avi", kVideoIdrs),
    std::make_pair(".m4v", kVideoIdrs),
    std::make_pair(".mov", kVideoIdrs),
    std::make_pair(".mp4", kVideoIdrs),
    std::make_pair(".mpeg", kVideoIdrs),
    std::make_pair(".mpg", kVideoIdrs),
    std::make_pair(".mpeg4", kVideoIdrs),
    std::make_pair(".mpg4", kVideoIdrs),
#endif
    std::make_pair(".flac", kAudioIdrs),
    std::make_pair(".oga", kAudioIdrs),
    std::make_pair(".ogg", kAudioIdrs),
    std::make_pair(".wav", kAudioIdrs),
    std::make_pair(".htm", kHtmlIdrs),
    std::make_pair(".html", kHtmlIdrs),
    std::make_pair(".bmp", kImageIdrs),
    std::make_pair(".gif", kImageIdrs),
    std::make_pair(".ico", kImageIdrs),
    std::make_pair(".jpeg", kImageIdrs),
    std::make_pair(".jpg", kImageIdrs),
    std::make_pair(".png", kImageIdrs),
    std::make_pair(".webp", kImageIdrs),
    std::make_pair(".log", kTextIdrs),
    std::make_pair(".pod", kTextIdrs),
    std::make_pair(".rst", kTextIdrs),
    std::make_pair(".txt", kTextIdrs),
    std::make_pair(".ogm", kVideoIdrs),
    std::make_pair(".ogv", kVideoIdrs),
    std::make_pair(".ogx", kVideoIdrs),
    std::make_pair(".webm", kVideoIdrs),
  };

  const size_t kESize = arraysize(kExtensionIdrBySizeData);
  return ExtensionIconSizeMap(&kExtensionIdrBySizeData[0],
                              &kExtensionIdrBySizeData[kESize]);
}

// Split on the very first &. The first part is path, the rest query.
void GetExtensionAndQuery(const std::string& url,
                          std::string* extension,
                          std::string* query) {
  // We receive the url with chrome://fileicon/ stripped but GURL expects it.
  const GURL gurl("chrome://fileicon/" + net::EscapePath(url));
  const std::string path = gurl.path();
  *extension = StringToLowerASCII(FilePath(path).Extension());
  *query = gurl.query();
}

// Simple parser for data on the query.
IconLoader::IconSize QueryToIconSize(const std::string& query) {
  CR_DEFINE_STATIC_LOCAL(
      QueryIconSizeMap, kQueryIconSizeMap, (BuildQueryIconSizeMap()));
  typedef std::pair<std::string, std::string> KVPair;
  std::vector<KVPair> parameters;
  if (base::SplitStringIntoKeyValuePairs(query, '=', '&', &parameters)) {
    for (std::vector<KVPair>::const_iterator itk = parameters.begin();
         itk != parameters.end(); ++itk) {
      if (itk->first == kIconSize) {
        QueryIconSizeMap::const_iterator itq(
            kQueryIconSizeMap.find(itk->second));
        if (itq != kQueryIconSizeMap.end())
          return itq->second;
      }
    }
  }
  return IconLoader::NORMAL;
}

// Finds matching resource of proper size. Fallback to generic.
int UrlToIDR(const std::string& url) {
  CR_DEFINE_STATIC_LOCAL(
      ExtensionIconSizeMap, kExtensionIdrSizeMap, (BuildExtensionIdrSizeMap()));
  std::string extension, query;
  int idr = -1;
  GetExtensionAndQuery(url, &extension, &query);
  const IconLoader::IconSize size = QueryToIconSize(query);
  ExtensionIconSizeMap::const_iterator it =
      kExtensionIdrSizeMap.find(extension);
  if (it != kExtensionIdrSizeMap.end()) {
    IdrBySize idrbysize = it->second;
    if (size == IconLoader::SMALL) {
      idr = idrbysize.idr_small_;
    } else if (size == IconLoader::NORMAL) {
      idr = idrbysize.idr_normal_;
    } else if (size == IconLoader::LARGE) {
      idr = idrbysize.idr_large_;
    }
  }
  if (idr == -1) {
    if (size == IconLoader::SMALL) {
      idr = kGenericIdrs.idr_small_;
    } else if (size == IconLoader::NORMAL) {
      idr = kGenericIdrs.idr_normal_;
    } else {
      idr = kGenericIdrs.idr_large_;
    }
  }
  return idr;
}
}  // namespace

FileIconSourceCros::FileIconSourceCros()
    : DataSource("fileicon", NULL) {
}

FileIconSourceCros::~FileIconSourceCros() {
}

void FileIconSourceCros::StartDataRequest(const std::string& url,
                                          bool is_incognito,
                                          int request_id) {
  int idr = UrlToIDR(url);
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<RefCountedStaticMemory> bytes(rb.LoadDataResourceBytes(idr));
  SendResponse(request_id, bytes);
}

// The mime type refers to the type of the response/icon served.
std::string FileIconSourceCros::GetMimeType(
    const std::string& url) const {
  return "image/png";
}
