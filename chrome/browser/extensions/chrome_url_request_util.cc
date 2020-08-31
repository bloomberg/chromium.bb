// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_url_request_util.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/file_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"

using extensions::ExtensionsBrowserClient;

namespace {

void DetermineCharset(const std::string& mime_type,
                      const base::RefCountedMemory* data,
                      std::string* out_charset) {
  if (base::StartsWith(mime_type, "text/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // All of our HTML files should be UTF-8 and for other resource types
    // (like images), charset doesn't matter.
    DCHECK(base::IsStringUTF8(base::StringPiece(
        reinterpret_cast<const char*>(data->front()), data->size())));
    *out_charset = "utf-8";
  }
}

scoped_refptr<base::RefCountedMemory> GetResource(
    int resource_id,
    const std::string& extension_id) {
  const ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  scoped_refptr<base::RefCountedMemory> bytes =
      rb.LoadDataResourceBytes(resource_id);
  auto* replacements =
      ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager()
          ? ExtensionsBrowserClient::Get()
                ->GetComponentExtensionResourceManager()
                ->GetTemplateReplacementsForExtension(extension_id)
          : nullptr;

  if (replacements) {
    base::StringPiece input(reinterpret_cast<const char*>(bytes->front()),
                            bytes->size());
    std::string temp_str = ui::ReplaceTemplateExpressions(input, *replacements);
    DCHECK(!temp_str.empty());
    return base::RefCountedString::TakeString(&temp_str);
  } else {
    return bytes;
  }
}

// Loads an extension resource in a Chrome .pak file. These are used by
// component extensions.
class ResourceBundleFileLoader : public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info,
      const base::FilePath& filename,
      int resource_id,
      const std::string& content_security_policy,
      bool send_cors_header) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClient
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* bundle_loader =
        new ResourceBundleFileLoader(content_security_policy, send_cors_header);
    bundle_loader->Start(request, std::move(loader), std::move(client_info),
                         filename, resource_id);
  }

  // mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override {
    NOTREACHED() << "No redirects for local file loads.";
  }
  // Current implementation reads all resource data at start of resource
  // load, so priority, and pausing is not currently implemented.
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  ResourceBundleFileLoader(const std::string& content_security_policy,
                           bool send_cors_header) {
    response_headers_ = extensions::BuildHttpHeaders(
        content_security_policy, send_cors_header, base::Time());
  }
  ~ResourceBundleFileLoader() override = default;

  void Start(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info_remote,
      const base::FilePath& filename,
      int resource_id) {
    client_.Bind(std::move(client_info_remote));
    receiver_.Bind(std::move(loader));
    receiver_.set_disconnect_handler(base::BindOnce(
        &ResourceBundleFileLoader::OnReceiverError, base::Unretained(this)));
    client_.set_disconnect_handler(base::BindOnce(
        &ResourceBundleFileLoader::OnMojoDisconnect, base::Unretained(this)));
    auto data = GetResource(resource_id, request.url.host());

    std::string* read_mime_type = new std::string;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&net::GetMimeTypeFromFile, filename,
                       base::Unretained(read_mime_type)),
        base::BindOnce(&ResourceBundleFileLoader::OnMimeTypeRead,
                       weak_factory_.GetWeakPtr(), std::move(data),
                       base::Owned(read_mime_type)));
  }

  void OnMimeTypeRead(scoped_refptr<base::RefCountedMemory> data,
                      std::string* read_mime_type,
                      bool read_result) {
    auto head = network::mojom::URLResponseHead::New();
    head->request_start = base::TimeTicks::Now();
    head->response_start = base::TimeTicks::Now();
    head->content_length = data->size();
    head->mime_type = *read_mime_type;
    DetermineCharset(head->mime_type, data.get(), &head->charset);
    mojo::DataPipe pipe(data->size());
    if (!pipe.consumer_handle.is_valid()) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      client_.reset();
      MaybeDeleteSelf();
      return;
    }
    head->headers = response_headers_;
    head->headers->AddHeader(net::HttpRequestHeaders::kContentLength,
                             base::NumberToString(head->content_length));
    if (!head->mime_type.empty()) {
      head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                               head->mime_type.c_str());
    }
    client_->OnReceiveResponse(std::move(head));
    client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));

    uint32_t write_size = data->size();
    MojoResult result = pipe.producer_handle->WriteData(
        data->front(), &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    OnFileWritten(result);
  }

  void OnMojoDisconnect() {
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnReceiverError() {
    receiver_.reset();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!receiver_.is_bound() && !client_.is_bound())
      delete this;
  }

  void OnFileWritten(MojoResult result) {
    // All the data has been written now. The consumer will be notified that
    // there will be no more data to read from now.
    if (result == MOJO_RESULT_OK)
      client_->OnComplete(network::URLLoaderCompletionStatus(net::OK));
    else
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    client_.reset();
    MaybeDeleteSelf();
  }

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  base::WeakPtrFactory<ResourceBundleFileLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResourceBundleFileLoader);
};

}  // namespace

namespace extensions {
namespace chrome_url_request_util {

bool AllowCrossRendererResourceLoad(const GURL& url,
                                    blink::mojom::ResourceType resource_type,
                                    ui::PageTransition page_transition,
                                    int child_id,
                                    bool is_incognito,
                                    const Extension* extension,
                                    const ExtensionSet& extensions,
                                    const ProcessMap& process_map,
                                    bool* allowed) {
  if (url_request_util::AllowCrossRendererResourceLoad(
          url, resource_type, page_transition, child_id, is_incognito,
          extension, extensions, process_map, allowed)) {
    return true;
  }

  // If there aren't any explicitly marked web accessible resources, the
  // load should be allowed only if it is by DevTools. A close approximation is
  // checking if the extension contains a DevTools page.
  if (extension &&
      !chrome_manifest_urls::GetDevToolsPage(extension).is_empty()) {
    *allowed = true;
    return true;
  }

  // Couldn't determine if the resource is allowed or not.
  return false;
}

base::FilePath GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) {
  *resource_id = 0;
  // |chrome_resources_path| corresponds to src/chrome/browser/resources in
  // source tree.
  base::FilePath chrome_resources_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &chrome_resources_path))
    return base::FilePath();

  // Since component extension resources are included in
  // component_extension_resources.pak file in |chrome_resources_path|,
  // calculate the extension |request_relative_path| against
  // |chrome_resources_path|.
  if (!chrome_resources_path.IsParent(extension_resources_path))
    return base::FilePath();

  const base::FilePath request_relative_path =
      extensions::file_util::ExtensionURLToRelativeFilePath(request.url);
  if (!ExtensionsBrowserClient::Get()
           ->GetComponentExtensionResourceManager()
           ->IsComponentExtensionResource(extension_resources_path,
                                          request_relative_path, resource_id)) {
    return base::FilePath();
  }
  DCHECK_NE(0, *resource_id);

  return request_relative_path;
}

void LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    const std::string& content_security_policy,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    bool send_cors_header) {
  DCHECK(!resource_relative_path.empty());
  ResourceBundleFileLoader::CreateAndStart(
      request, std::move(loader), std::move(client), resource_relative_path,
      resource_id, content_security_policy, send_cors_header);
}

}  // namespace chrome_url_request_util
}  // namespace extensions
