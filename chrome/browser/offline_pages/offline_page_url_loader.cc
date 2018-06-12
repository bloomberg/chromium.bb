// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_url_loader.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"

namespace offline_pages {

namespace {

constexpr size_t kBufferSize = 4096;

content::WebContents* GetWebContents(int frame_tree_node_id) {
  return content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
}

bool GetTabId(content::WebContents* web_contents, int* tab_id) {
  return OfflinePageUtils::GetTabId(web_contents, tab_id);
}

net::RedirectInfo CreateRedirectInfo(const GURL& redirected_url,
                                     int response_code) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = redirected_url;
  redirect_info.new_referrer_policy = net::URLRequest::NO_REFERRER;
  redirect_info.new_method = "GET";
  redirect_info.status_code = response_code;
  redirect_info.new_site_for_cookies = redirect_info.new_url;
  return redirect_info;
}

bool ShouldCreateLoader(const network::ResourceRequest& resource_request) {
  // Ignore the requests not for the main frame.
  if (resource_request.resource_type != content::RESOURCE_TYPE_MAIN_FRAME)
    return false;

  // Ignore non-http/https requests.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS())
    return false;

  // Ignore requests other than GET.
  if (resource_request.method != "GET")
    return false;

  return true;
}

}  // namespace

// static
std::unique_ptr<OfflinePageURLLoader> OfflinePageURLLoader::Create(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    const network::ResourceRequest& resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  if (ShouldCreateLoader(resource_request)) {
    return base::WrapUnique(
        new OfflinePageURLLoader(navigation_ui_data, frame_tree_node_id,
                                 resource_request, std::move(callback)));
  }

  std::move(callback).Run({});
  return nullptr;
}

OfflinePageURLLoader::OfflinePageURLLoader(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    const network::ResourceRequest& resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback)
    : navigation_ui_data_(navigation_ui_data),
      frame_tree_node_id_(frame_tree_node_id),
      transition_type_(resource_request.transition_type),
      loader_callback_(std::move(callback)),
      binding_(this),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  request_handler_ = std::make_unique<OfflinePageRequestHandler>(
      resource_request.url, resource_request.headers, this);
  request_handler_->Start();
}

OfflinePageURLLoader::~OfflinePageURLLoader() {}

void OfflinePageURLLoader::SetTabIdGetterForTesting(
    OfflinePageRequestHandler::Delegate::TabIdGetter tab_id_getter) {
  tab_id_getter_ = tab_id_getter;
}

void OfflinePageURLLoader::SetShouldAllowPreviewCallbackForTesting(
    ShouldAllowPreviewCallback should_allow_preview_callback) {
  should_allow_preview_callback_ = should_allow_preview_callback;
}

void OfflinePageURLLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  NOTREACHED();
}

void OfflinePageURLLoader::ProceedWithResponse() {
  NOTREACHED();
}

void OfflinePageURLLoader::SetPriority(net::RequestPriority priority,
                                       int32_t intra_priority_value) {
  // Ignore: this class doesn't have a concept of priority.
}

void OfflinePageURLLoader::PauseReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void OfflinePageURLLoader::ResumeReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void OfflinePageURLLoader::FallbackToDefault() {
  std::move(loader_callback_).Run({});
}

void OfflinePageURLLoader::NotifyStartError(int error) {
  std::move(loader_callback_)
      .Run(base::BindOnce(&OfflinePageURLLoader::OnReceiveError,
                          weak_ptr_factory_.GetWeakPtr(), error));
}

void OfflinePageURLLoader::NotifyHeadersComplete(int64_t file_size) {
  std::move(loader_callback_)
      .Run(base::BindOnce(&OfflinePageURLLoader::OnReceiveResponse,
                          weak_ptr_factory_.GetWeakPtr(), file_size));
}

void OfflinePageURLLoader::NotifyReadRawDataComplete(int bytes_read) {
  if (bytes_read < 0) {
    // Negative |bytes_read| is net error code.
    Finish(bytes_read);
    return;
  }
  if (bytes_read == 0) {
    // Zero |bytes_read| means reaching EOF.
    Finish(net::OK);
    return;
  }

  bytes_of_raw_data_to_transfer_ = bytes_read;
  write_position_ = 0;

  TransferRawData();
}

void OfflinePageURLLoader::TransferRawData() {
  while (true) {
    DCHECK_GE(bytes_of_raw_data_to_transfer_, write_position_);
    uint32_t write_size =
        static_cast<uint32_t>(bytes_of_raw_data_to_transfer_ - write_position_);
    // If all the read data have been transferred, read more.
    if (write_size == 0) {
      ReadRawData();
      return;
    }

    MojoResult result =
        producer_handle_->WriteData(buffer_->data() + write_position_,
                                    &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      Finish(net::ERR_FAILED);
      return;
    }

    write_position_ += write_size;
  }
}

void OfflinePageURLLoader::SetOfflinePageNavigationUIData(
    bool is_offline_page) {
  // This method should be called before the response data is received.
  DCHECK(!binding_.is_bound());

  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data_);
  std::unique_ptr<OfflinePageNavigationUIData> offline_page_data =
      std::make_unique<OfflinePageNavigationUIData>(is_offline_page);
  navigation_data->SetOfflinePageNavigationUIData(std::move(offline_page_data));
}

bool OfflinePageURLLoader::ShouldAllowPreview() const {
  // TODO(jianli): This is a temporary hack to make the tests pass. The real
  // logic needs to be implemented.
  if (!should_allow_preview_callback_.is_null())
    return should_allow_preview_callback_.Run();
  return false;
}

int OfflinePageURLLoader::GetPageTransition() const {
  return transition_type_;
}

OfflinePageRequestHandler::Delegate::WebContentsGetter
OfflinePageURLLoader::GetWebContentsGetter() const {
  return base::Bind(&GetWebContents, frame_tree_node_id_);
}

OfflinePageRequestHandler::Delegate::TabIdGetter
OfflinePageURLLoader::GetTabIdGetter() const {
  if (!tab_id_getter_.is_null())
    return tab_id_getter_;
  return base::Bind(&GetTabId);
}

void OfflinePageURLLoader::ReadRawData() {
  int result = request_handler_->ReadRawData(buffer_.get(), kBufferSize);
  // If |result| is not ERR_IO_PENDING, the read data is available immediately.
  // Otherwise, the read is asynchronous and NotifyReadRawDataComplete will
  // be invoked when the read finishes.
  if (result != net::ERR_IO_PENDING)
    NotifyReadRawDataComplete(result);
}

void OfflinePageURLLoader::OnReceiveError(
    int error,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  client_ = std::move(client);
  Finish(error);
}

void OfflinePageURLLoader::OnReceiveResponse(
    int64_t file_size,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&OfflinePageURLLoader::OnConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));
  client_ = std::move(client);

  mojo::DataPipe pipe(kBufferSize);
  if (!pipe.consumer_handle.is_valid()) {
    Finish(net::ERR_FAILED);
    return;
  }

  network::ResourceResponseHead response_head;
  response_head.request_start = base::TimeTicks::Now();
  response_head.response_start = response_head.request_start;

  scoped_refptr<net::HttpResponseHeaders> redirect_headers =
      request_handler_->GetRedirectHeaders();
  if (redirect_headers.get()) {
    std::string redirected_url;
    bool is_redirect = redirect_headers->IsRedirect(&redirected_url);
    DCHECK(is_redirect);
    response_head.headers = redirect_headers;
    response_head.encoded_data_length = 0;
    client_->OnReceiveRedirect(
        CreateRedirectInfo(GURL(redirected_url),
                           redirect_headers->response_code()),
        response_head);
    return;
  }

  response_head.mime_type = "multipart/related";
  response_head.charset = "utf-8";
  response_head.content_length = file_size;

  client_->OnReceiveResponse(response_head);
  client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));

  producer_handle_ = std::move(pipe.producer_handle);

  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunnerHandle::Get());
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&OfflinePageURLLoader::OnHandleReady,
                          weak_ptr_factory_.GetWeakPtr()));

  buffer_ = base::MakeRefCounted<net::IOBuffer>(kBufferSize);
  ReadRawData();
}

void OfflinePageURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }
  TransferRawData();
}

void OfflinePageURLLoader::Finish(int error) {
  client_->OnComplete(network::URLLoaderCompletionStatus(error));
  handle_watcher_.reset();
  producer_handle_.reset();
  client_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  MaybeDeleteSelf();
}

void OfflinePageURLLoader::OnConnectionError() {
  binding_.Close();
  client_.reset();
  MaybeDeleteSelf();
}

void OfflinePageURLLoader::MaybeDeleteSelf() {
  if (!binding_.is_bound() && !client_.is_bound())
    delete this;
}

}  // namespace offline_pages
