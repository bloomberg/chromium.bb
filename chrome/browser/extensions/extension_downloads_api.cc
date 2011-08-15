// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_downloads_api.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_file_manager.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_downloads_api_constants.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"

namespace constants = extension_downloads_api_constants;

DownloadsDownloadFunction::DownloadsDownloadFunction()
  : save_as_(false),
    extra_headers_(NULL),
    rdh_(NULL),
    resource_context_(NULL),
    render_process_host_id_(0),
    render_view_host_routing_id_(0) {
}
DownloadsDownloadFunction::~DownloadsDownloadFunction() {}

bool DownloadsDownloadFunction::RunImpl() {
  base::DictionaryValue* options = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  EXTENSION_FUNCTION_VALIDATE(options->GetString(constants::kUrlKey, &url_));
  if (options->HasKey(constants::kFilenameKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        constants::kFilenameKey, &filename_));
  if (options->HasKey(constants::kSaveAsKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetBoolean(
        constants::kSaveAsKey, &save_as_));
  if (options->HasKey(constants::kMethodKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        constants::kMethodKey, &method_));
  if (options->HasKey(constants::kHeadersKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetDictionary(
        constants::kHeadersKey, &extra_headers_));
  if (options->HasKey(constants::kBodyKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        constants::kBodyKey, &post_body_));
  rdh_ = g_browser_process->resource_dispatcher_host();
  TabContents* tab_contents = BrowserList::GetLastActive()
    ->GetSelectedTabContentsWrapper()->tab_contents();
  resource_context_ = &profile()->GetResourceContext();
  render_process_host_id_ = tab_contents->GetRenderProcessHost()->id();
  render_view_host_routing_id_ = tab_contents->render_view_host()
    ->routing_id();
  VLOG(1) << __FUNCTION__ << " " << url_;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsSearchFunction::DownloadsSearchFunction() {}
DownloadsSearchFunction::~DownloadsSearchFunction() {}

bool DownloadsSearchFunction::RunImpl() {
  DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsPauseFunction::DownloadsPauseFunction() {}
DownloadsPauseFunction::~DownloadsPauseFunction() {}

bool DownloadsPauseFunction::RunImpl() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsResumeFunction::DownloadsResumeFunction() {}
DownloadsResumeFunction::~DownloadsResumeFunction() {}

bool DownloadsResumeFunction::RunImpl() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsCancelFunction::DownloadsCancelFunction() {}
DownloadsCancelFunction::~DownloadsCancelFunction() {}

bool DownloadsCancelFunction::RunImpl() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsEraseFunction::DownloadsEraseFunction() {}
DownloadsEraseFunction::~DownloadsEraseFunction() {}

bool DownloadsEraseFunction::RunImpl() {
  DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsSetDestinationFunction::DownloadsSetDestinationFunction() {}
DownloadsSetDestinationFunction::~DownloadsSetDestinationFunction() {}

bool DownloadsSetDestinationFunction::RunImpl() {
  int dl_id = 0;
  std::string path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &path));
  VLOG(1) << __FUNCTION__ << " " << dl_id << " " << &path;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction() {}
DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

bool DownloadsAcceptDangerFunction::RunImpl() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsShowFunction::DownloadsShowFunction() {}
DownloadsShowFunction::~DownloadsShowFunction() {}

bool DownloadsShowFunction::RunImpl() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

DownloadsDragFunction::DownloadsDragFunction() {}
DownloadsDragFunction::~DownloadsDragFunction() {}

bool DownloadsDragFunction::RunImpl() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}
