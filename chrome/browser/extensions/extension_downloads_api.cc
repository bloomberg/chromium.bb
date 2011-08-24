// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_downloads_api.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_downloads_api_constants.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_item.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"

namespace constants = extension_downloads_api_constants;

bool DownloadsFunctionInterface::RunImplImpl(
    DownloadsFunctionInterface* pimpl) {
  CHECK(pimpl);
  if (!pimpl->ParseArgs()) return false;
  UMA_HISTOGRAM_ENUMERATION(
      "Download.ApiFunctions", pimpl->function(), DOWNLOADS_FUNCTION_LAST);
  pimpl->RunInternal();
  return true;
}

SyncDownloadsFunction::SyncDownloadsFunction(
    DownloadsFunctionInterface::DownloadsFunctionName function)
  : function_(function) {
}

SyncDownloadsFunction::~SyncDownloadsFunction() {}

bool SyncDownloadsFunction::RunImpl() {
  return DownloadsFunctionInterface::RunImplImpl(this);
}

DownloadsFunctionInterface::DownloadsFunctionName
SyncDownloadsFunction::function() const {
  return function_;
}

AsyncDownloadsFunction::AsyncDownloadsFunction(
    DownloadsFunctionInterface::DownloadsFunctionName function)
  : function_(function) {
}

AsyncDownloadsFunction::~AsyncDownloadsFunction() {}

bool AsyncDownloadsFunction::RunImpl() {
  return DownloadsFunctionInterface::RunImplImpl(this);
}

DownloadsFunctionInterface::DownloadsFunctionName
AsyncDownloadsFunction::function() const {
  return function_;
}

DownloadsDownloadFunction::DownloadsDownloadFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_DOWNLOAD),
    save_as_(false),
    extra_headers_(NULL),
    rdh_(NULL),
    resource_context_(NULL),
    render_process_host_id_(0),
    render_view_host_routing_id_(0) {
}

DownloadsDownloadFunction::~DownloadsDownloadFunction() {}

bool DownloadsDownloadFunction::ParseArgs() {
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

void DownloadsDownloadFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsSearchFunction::DownloadsSearchFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_SEARCH) {
}

DownloadsSearchFunction::~DownloadsSearchFunction() {}

bool DownloadsSearchFunction::ParseArgs() {
  DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsSearchFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsPauseFunction::DownloadsPauseFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_PAUSE) {
}

DownloadsPauseFunction::~DownloadsPauseFunction() {}

bool DownloadsPauseFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsPauseFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsResumeFunction::DownloadsResumeFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_RESUME) {
}

DownloadsResumeFunction::~DownloadsResumeFunction() {}

bool DownloadsResumeFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsResumeFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsCancelFunction::DownloadsCancelFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_CANCEL) {
}

DownloadsCancelFunction::~DownloadsCancelFunction() {}

bool DownloadsCancelFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsCancelFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsEraseFunction::DownloadsEraseFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_ERASE) {
}

DownloadsEraseFunction::~DownloadsEraseFunction() {}

bool DownloadsEraseFunction::ParseArgs() {
  DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsEraseFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsSetDestinationFunction::DownloadsSetDestinationFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_SET_DESTINATION) {
}

DownloadsSetDestinationFunction::~DownloadsSetDestinationFunction() {}

bool DownloadsSetDestinationFunction::ParseArgs() {
  int dl_id = 0;
  std::string path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &path));
  VLOG(1) << __FUNCTION__ << " " << dl_id << " " << &path;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsSetDestinationFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_ACCEPT_DANGER) {
}

DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

bool DownloadsAcceptDangerFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsAcceptDangerFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsShowFunction::DownloadsShowFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_SHOW) {
}

DownloadsShowFunction::~DownloadsShowFunction() {}

bool DownloadsShowFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsShowFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsDragFunction::DownloadsDragFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_DRAG) {
}

DownloadsDragFunction::~DownloadsDragFunction() {}

bool DownloadsDragFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = constants::kNotImplemented;
  return false;
}

void DownloadsDragFunction::RunInternal() {
  NOTIMPLEMENTED();
}
