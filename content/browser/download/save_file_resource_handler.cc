// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file_resource_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/download/download_task_runner.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/loader/resource_controller.h"
#include "net/base/io_buffer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_status.h"

namespace content {

SaveFileResourceHandler::SaveFileResourceHandler(
    net::URLRequest* request,
    SaveItemId save_item_id,
    SavePackageId save_package_id,
    int render_process_host_id,
    int render_frame_routing_id,
    const GURL& url,
    AuthorizationState authorization_state)
    : ResourceHandler(request),
      save_item_id_(save_item_id),
      save_package_id_(save_package_id),
      render_process_id_(render_process_host_id),
      render_frame_routing_id_(render_frame_routing_id),
      url_(url),
      content_length_(0),
      save_manager_(SaveFileManager::Get()),
      authorization_state_(authorization_state) {}

SaveFileResourceHandler::~SaveFileResourceHandler() {
}

void SaveFileResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  final_url_ = redirect_info.new_url;
  controller->Resume();
}

void SaveFileResourceHandler::OnResponseStarted(
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  // |save_manager_| consumes (deletes):
  SaveFileCreateInfo* info = new SaveFileCreateInfo(
      url_, final_url_, save_item_id_, save_package_id_, render_process_id_,
      render_frame_routing_id_, GetRequestID(), content_disposition_,
      content_length_);
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveFileManager::StartSave, save_manager_, info));
  controller->Resume();
}

void SaveFileResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  if (authorization_state_ == AuthorizationState::AUTHORIZED) {
    controller->Resume();
  } else {
    controller->Cancel();
  }
}

void SaveFileResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  DCHECK_EQ(AuthorizationState::AUTHORIZED, authorization_state_);
  DCHECK(buf && buf_size);
  if (!read_buffer_.get()) {
    *buf_size = kReadBufSize;
    read_buffer_ = new net::IOBuffer(*buf_size);
  }
  *buf = read_buffer_.get();
  controller->Resume();
}

void SaveFileResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK_EQ(AuthorizationState::AUTHORIZED, authorization_state_);
  DCHECK(read_buffer_.get());
  // We are passing ownership of this buffer to the save file manager.
  scoped_refptr<net::IOBuffer> buffer;
  read_buffer_.swap(buffer);
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveFileManager::UpdateSaveProgress, save_manager_,
                     save_item_id_, base::RetainedRef(buffer), bytes_read));
  controller->Resume();
}

void SaveFileResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  if (authorization_state_ != AuthorizationState::AUTHORIZED)
    DCHECK(!status.is_success());

  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveFileManager::SaveFinished, save_manager_,
                     save_item_id_, save_package_id_,
                     status.is_success() && !status.is_io_pending()));
  read_buffer_ = nullptr;
  controller->Resume();
}

void SaveFileResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void SaveFileResourceHandler::set_content_length(
    const std::string& content_length) {
  base::StringToInt64(content_length, &content_length_);
}

}  // namespace content
