// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_url_loader_factory.h"

#include <map>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/sys_byteorder.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/template_expressions.h"

namespace content {

namespace {

class WebUIURLLoaderFactory;
base::LazyInstance<std::map<int, std::unique_ptr<WebUIURLLoaderFactory>>>::Leaky
    g_factories = LAZY_INSTANCE_INITIALIZER;

class URLLoaderImpl : public mojom::URLLoader {
 public:
  static void Create(mojom::URLLoaderAssociatedRequest loader,
                     const ResourceRequest& request,
                     int frame_tree_node_id,
                     mojo::InterfacePtrInfo<mojom::URLLoaderClient> client_info,
                     ResourceContext* resource_context) {
    mojom::URLLoaderClientPtr client;
    client.Bind(std::move(client_info));
    new URLLoaderImpl(std::move(loader), request, frame_tree_node_id,
                      std::move(client), resource_context);
  }

 private:
  URLLoaderImpl(mojom::URLLoaderAssociatedRequest loader,
                const ResourceRequest& request,
                int frame_tree_node_id,
                mojom::URLLoaderClientPtr client,
                ResourceContext* resource_context)
      : binding_(this, std::move(loader)),
        client_(std::move(client)),
        replacements_(nullptr),
        weak_factory_(this) {
    // NOTE: this duplicates code in URLDataManagerBackend::StartRequest.
    if (!URLDataManagerBackend::CheckURLIsValid(request.url)) {
      OnError(net::ERR_INVALID_URL);
      return;
    }

    URLDataSourceImpl* source =
        GetURLDataManagerForResourceContext(resource_context)
            ->GetDataSourceFromURL(request.url);
    if (!source) {
      OnError(net::ERR_INVALID_URL);
      return;
    }

    if (!source->source()->ShouldServiceRequest(request.url, resource_context,
                                                -1)) {
      OnError(net::ERR_INVALID_URL);
      return;
    }

    std::string path;
    URLDataManagerBackend::URLToRequestPath(request.url, &path);
    gzipped_ = source->source()->IsGzipped(path);
    if (source->source()->GetMimeType(path) == "text/html")
      replacements_ = source->GetReplacements();

    net::HttpRequestHeaders request_headers;
    request_headers.AddHeadersFromString(request.headers);
    std::string origin_header;
    request_headers.GetHeader(net::HttpRequestHeaders::kOrigin, &origin_header);

    scoped_refptr<net::HttpResponseHeaders> headers =
        URLDataManagerBackend::GetHeaders(source, path, origin_header);

    ResourceResponseHead head;
    head.headers = headers;
    head.mime_type = source->source()->GetMimeType(path);
    // TODO: fill all the time related field i.e. request_time response_time
    // request_start response_start
    client_->OnReceiveResponse(head, base::nullopt, nullptr);

    ResourceRequestInfo::WebContentsGetter wc_getter =
        base::Bind(WebContents::FromFrameTreeNodeId, frame_tree_node_id);

    // Forward along the request to the data source.
    // TODO(jam): once we only have this code path for WebUI, and not the
    // URLLRequestJob one, then we should switch data sources to run on the UI
    // thread by default.
    scoped_refptr<base::SingleThreadTaskRunner> target_runner =
        source->source()->TaskRunnerForRequestPath(path);
    if (!target_runner) {
      source->source()->StartDataRequest(
          path, wc_getter,
          base::Bind(&URLLoaderImpl::DataAvailable,
                     weak_factory_.GetWeakPtr()));
    } else {
      // The DataSource wants StartDataRequest to be called on a specific
      // thread, usually the UI thread, for this path.
      target_runner->PostTask(
          FROM_HERE, base::Bind(&URLLoaderImpl::CallStartDataRequest,
                                base::RetainedRef(source), path, wc_getter,
                                weak_factory_.GetWeakPtr()));
    }
  }

  void FollowRedirect() override { NOTREACHED(); }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    NOTREACHED();
  }

  static void CallStartDataRequest(
      scoped_refptr<URLDataSourceImpl> source,
      const std::string& path,
      const ResourceRequestInfo::WebContentsGetter& wc_getter,
      const base::WeakPtr<URLLoaderImpl>& weak_ptr) {
    source->source()->StartDataRequest(
        path, wc_getter,
        base::Bind(URLLoaderImpl::DataAvailableOnTargetThread, weak_ptr));
  }

  static void DataAvailableOnTargetThread(
      const base::WeakPtr<URLLoaderImpl>& weak_ptr,
      scoped_refptr<base::RefCountedMemory> bytes) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLLoaderImpl::DataAvailable, weak_ptr, bytes));
  }

  void DataAvailable(scoped_refptr<base::RefCountedMemory> bytes) {
    if (!bytes) {
      OnError(net::ERR_FAILED);
      return;
    }

    base::StringPiece input(reinterpret_cast<const char*>(bytes->front()),
                            bytes->size());
    if (replacements_) {
      std::string temp_string;
      // We won't know the the final output size ahead of time, so we have to
      // use an intermediate string.
      base::StringPiece source;
      std::string temp_str;
      if (gzipped_) {
        temp_str.resize(compression::GetUncompressedSize(input));
        source.set(temp_str.c_str(), temp_str.size());
        CHECK(compression::GzipUncompress(input, source));
        gzipped_ = false;
      } else {
        source = input;
      }
      temp_str = ui::ReplaceTemplateExpressions(source, *replacements_);
      bytes = base::RefCountedString::TakeString(&temp_str);
      input.set(reinterpret_cast<const char*>(bytes->front()), bytes->size());
    }

    uint32_t output_size =
        gzipped_ ? compression::GetUncompressedSize(input) : bytes->size();

    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = output_size;
    mojo::DataPipe data_pipe(options);

    DCHECK(data_pipe.producer_handle.is_valid());
    DCHECK(data_pipe.consumer_handle.is_valid());

    void* buffer = nullptr;
    uint32_t num_bytes = output_size;
    MojoResult result =
        BeginWriteDataRaw(data_pipe.producer_handle.get(), &buffer, &num_bytes,
                          MOJO_WRITE_DATA_FLAG_NONE);
    CHECK_EQ(result, MOJO_RESULT_OK);
    CHECK_EQ(num_bytes, output_size);

    if (gzipped_) {
      base::StringPiece output(static_cast<char*>(buffer), num_bytes);
      CHECK(compression::GzipUncompress(input, output));
    } else {
      memcpy(buffer, bytes->front(), output_size);
    }
    result = EndWriteDataRaw(data_pipe.producer_handle.get(), num_bytes);
    CHECK_EQ(result, MOJO_RESULT_OK);

    client_->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    ResourceRequestCompletionStatus request_complete_data;
    request_complete_data.error_code = net::OK;
    request_complete_data.exists_in_cache = false;
    request_complete_data.completion_time = base::TimeTicks::Now();
    request_complete_data.encoded_data_length = output_size;
    request_complete_data.encoded_body_length = output_size;
    client_->OnComplete(request_complete_data);
    delete this;
  }

  void OnError(int error_code) {
    ResourceRequestCompletionStatus status;
    status.error_code = error_code;
    client_->OnComplete(status);
    delete this;
  }

  mojo::AssociatedBinding<mojom::URLLoader> binding_;
  mojom::URLLoaderClientPtr client_;
  bool gzipped_;
  // Replacement dictionary for i18n.
  const ui::TemplateReplacements* replacements_;
  base::WeakPtrFactory<URLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderImpl);
};

class WebUIURLLoaderFactory : public mojom::URLLoaderFactory,
                              public FrameTreeNode::Observer {
 public:
  WebUIURLLoaderFactory(FrameTreeNode* ftn)
      : frame_tree_node_id_(ftn->frame_tree_node_id()),
        resource_context_(ftn->current_frame_host()
                              ->GetProcess()
                              ->GetBrowserContext()
                              ->GetResourceContext()) {
    ftn->AddObserver(this);
  }

  ~WebUIURLLoaderFactory() override {}

  mojom::URLLoaderFactoryPtr CreateBinding() {
    return loader_factory_bindings_.CreateInterfacePtrAndBind(this);
  }

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(mojom::URLLoaderAssociatedRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& request,
                            mojom::URLLoaderClientPtr client) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&URLLoaderImpl::Create, std::move(loader), request,
                       frame_tree_node_id_, client.PassInterface(),
                       resource_context_));
  }

  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& request,
                SyncLoadCallback callback) override {
    NOTREACHED();
  }

  // FrameTreeNode::Observer implementation:
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    g_factories.Get().erase(frame_tree_node_id_);
  }

 private:
  int frame_tree_node_id_;
  ResourceContext* resource_context_;
  mojo::BindingSet<mojom::URLLoaderFactory> loader_factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(WebUIURLLoaderFactory);
};

}  // namespace

mojom::URLLoaderFactoryPtr GetWebUIURLLoader(FrameTreeNode* node) {
  int ftn_id = node->frame_tree_node_id();
  if (g_factories.Get()[ftn_id].get() == nullptr)
    g_factories.Get()[ftn_id] = base::MakeUnique<WebUIURLLoaderFactory>(node);
  return g_factories.Get()[ftn_id]->CreateBinding();
}

}  // namespace content
