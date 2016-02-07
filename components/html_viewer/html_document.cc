// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_document.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/devtools_agent_impl.h"
#include "components/html_viewer/document_resource_waiter.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/test_html_viewer_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/ws/ids.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

using mojo::AxProvider;
using mus::Window;

namespace html_viewer {
namespace {

const char kEnableTestInterface[] = "enable-html-viewer-test-interface";

bool IsTestInterfaceEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTestInterface);
}

}  // namespace

// A WindowTreeDelegate implementation that delegates to a (swappable) delegate.
// This is used when one HTMLDocument takes over for another delegate
// (OnSwap()).
class WindowTreeDelegateImpl : public mus::WindowTreeDelegate {
 public:
  explicit WindowTreeDelegateImpl(mus::WindowTreeDelegate* delegate)
      : delegate_(delegate) {}
  ~WindowTreeDelegateImpl() override {}

  void set_delegate(mus::WindowTreeDelegate* delegate) { delegate_ = delegate; }

 private:
  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override { delegate_->OnEmbed(root); }
  void OnUnembed(mus::Window* root) override { delegate_->OnUnembed(root); }
  void OnConnectionLost(mus::WindowTreeConnection* connection) override {
    delegate_->OnConnectionLost(connection);
  }

  mus::WindowTreeDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeDelegateImpl);
};

HTMLDocument::BeforeLoadCache::BeforeLoadCache() {}

HTMLDocument::BeforeLoadCache::~BeforeLoadCache() {
  STLDeleteElements(&ax_provider_requests);
  STLDeleteElements(&test_interface_requests);
}

HTMLDocument::TransferableState::TransferableState()
    : owns_window_tree_connection(false), root(nullptr) {}

HTMLDocument::TransferableState::~TransferableState() {}

void HTMLDocument::TransferableState::Move(TransferableState* other) {
  owns_window_tree_connection = other->owns_window_tree_connection;
  root = other->root;
  window_tree_delegate_impl = std::move(other->window_tree_delegate_impl);

  other->root = nullptr;
  other->owns_window_tree_connection = false;
}

HTMLDocument::HTMLDocument(mojo::Shell* html_document_shell,
                           mojo::ApplicationConnection* connection,
                           mojo::URLResponsePtr response,
                           GlobalState* global_state,
                           const DeleteCallback& delete_callback,
                           HTMLFactory* factory)
    : app_refcount_(html_document_shell->CreateAppRefCount()),
      html_document_shell_(html_document_shell),
      connection_(connection),
      global_state_(global_state),
      frame_(nullptr),
      delete_callback_(delete_callback),
      factory_(factory) {
  connection->AddService<web_view::mojom::FrameClient>(this);
  connection->AddService<AxProvider>(this);
  connection->AddService<mus::mojom::WindowTreeClient>(this);
  connection->AddService<devtools_service::DevToolsAgent>(this);
  if (IsTestInterfaceEnabled())
    connection->AddService<TestHTMLViewer>(this);

  resource_waiter_.reset(
      new DocumentResourceWaiter(global_state_, std::move(response), this));
}

void HTMLDocument::Destroy() {
  TRACE_EVENT0("html_viewer", "HTMLDocument::Destroy");
  if (resource_waiter_) {
    mus::Window* root = resource_waiter_->root();
    if (root) {
      resource_waiter_.reset();
      delete root->connection();
    } else {
      delete this;
    }
  } else if (frame_) {
    // Closing the frame ends up destroying the ViewManager, which triggers
    // deleting this (OnConnectionLost()).
    frame_->Close();
  } else if (transferable_state_.root) {
    // This triggers deleting us.
    if (transferable_state_.owns_window_tree_connection)
      delete transferable_state_.root->connection();
    else
      delete this;
  } else {
    delete this;
  }
}

HTMLDocument::~HTMLDocument() {
  delete_callback_.Run(this);

  STLDeleteElements(&ax_providers_);
}

void HTMLDocument::Load() {
  TRACE_EVENT0("html_viewer", "HTMLDocument::Load");
  DCHECK(resource_waiter_ && resource_waiter_->is_ready());

  // Note: |window| is null if we're taking over for an existing frame.
  mus::Window* window = resource_waiter_->root();
  if (window) {
    global_state_->InitIfNecessary(
        window->viewport_metrics().size_in_pixels.To<gfx::Size>(),
        window->viewport_metrics().device_pixel_ratio);
  }

  scoped_ptr<WebURLRequestExtraData> extra_data(new WebURLRequestExtraData);
  extra_data->synthetic_response = resource_waiter_->ReleaseURLResponse();

  base::TimeTicks navigation_start_time =
      resource_waiter_->navigation_start_time();
  frame_ = HTMLFrameTreeManager::CreateFrameAndAttachToTree(
      global_state_, window, std::move(resource_waiter_), this);

  // If the frame wasn't created we can destroy ourself.
  if (!frame_) {
    Destroy();
    return;
  }

  if (devtools_agent_request_.is_pending()) {
    if (frame_->devtools_agent()) {
      frame_->devtools_agent()->BindToRequest(
          std::move(devtools_agent_request_));
    } else {
      devtools_agent_request_ =
          mojo::InterfaceRequest<devtools_service::DevToolsAgent>();
    }
  }

  const GURL url(extra_data->synthetic_response->url.get());

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data.release());

  frame_->LoadRequest(web_request, navigation_start_time);
}

HTMLDocument::BeforeLoadCache* HTMLDocument::GetBeforeLoadCache() {
  CHECK(!did_finish_local_frame_load_);
  if (!before_load_cache_.get())
    before_load_cache_.reset(new BeforeLoadCache);
  return before_load_cache_.get();
}

void HTMLDocument::OnEmbed(Window* root) {
  transferable_state_.root = root;
  resource_waiter_->SetRoot(root);
}

void HTMLDocument::OnConnectionLost(mus::WindowTreeConnection* connection) {
  delete this;
}

void HTMLDocument::OnFrameDidFinishLoad() {
  TRACE_EVENT0("html_viewer", "HTMLDocument::OnFrameDidFinishLoad");
  did_finish_local_frame_load_ = true;
  scoped_ptr<BeforeLoadCache> before_load_cache = std::move(before_load_cache_);
  if (!before_load_cache)
    return;

  // Bind any pending AxProvider and TestHTMLViewer interface requests.
  for (auto it : before_load_cache->ax_provider_requests) {
    ax_providers_.insert(new AxProviderImpl(
        frame_->frame_tree_manager()->GetWebView(), std::move(*it)));
  }
  for (auto it : before_load_cache->test_interface_requests) {
    CHECK(IsTestInterfaceEnabled());
    test_html_viewers_.push_back(new TestHTMLViewerImpl(
        frame_->web_frame()->toWebLocalFrame(), std::move(*it)));
  }
}

mojo::Shell* HTMLDocument::GetShell() {
  return html_document_shell_;
}

HTMLFactory* HTMLDocument::GetHTMLFactory() {
  return factory_;
}

void HTMLDocument::OnFrameSwappedToRemote() {
  // When the frame becomes remote HTMLDocument is no longer needed.
  frame_ = nullptr;
  Destroy();
}

void HTMLDocument::OnSwap(HTMLFrame* frame, HTMLFrameDelegate* old_delegate) {
  TRACE_EVENT0("html_viewer", "HTMLDocument::OnSwap");
  DCHECK(frame->IsLocal());
  DCHECK(frame->window());
  DCHECK(!frame_);
  DCHECK(!transferable_state_.root);
  if (!old_delegate) {
    // We're taking over a child of a local root that isn't associated with a
    // delegate. In this case the frame's window is not the root of the
    // WindowTreeConnection.
    transferable_state_.owns_window_tree_connection = false;
    transferable_state_.root = frame->window();
  } else {
    HTMLDocument* old_document = static_cast<HTMLDocument*>(old_delegate);
    transferable_state_.Move(&old_document->transferable_state_);
    if (transferable_state_.window_tree_delegate_impl)
      transferable_state_.window_tree_delegate_impl->set_delegate(this);
    old_document->frame_ = nullptr;
    old_document->Destroy();
  }
}

void HTMLDocument::OnFrameDestroyed() {
  if (!transferable_state_.owns_window_tree_connection)
    delete this;
}

void HTMLDocument::Create(mojo::ApplicationConnection* connection,
                          mojo::InterfaceRequest<AxProvider> request) {
  if (!did_finish_local_frame_load_) {
    // Cache AxProvider interface requests until the document finishes loading.
    auto cached_request = new mojo::InterfaceRequest<AxProvider>();
    *cached_request = std::move(request);
    GetBeforeLoadCache()->ax_provider_requests.insert(cached_request);
  } else {
    ax_providers_.insert(
        new AxProviderImpl(frame_->web_view(), std::move(request)));
  }
}

void HTMLDocument::Create(mojo::ApplicationConnection* connection,
                          mojo::InterfaceRequest<TestHTMLViewer> request) {
  CHECK(IsTestInterfaceEnabled());
  if (!did_finish_local_frame_load_) {
    auto cached_request = new mojo::InterfaceRequest<TestHTMLViewer>();
    *cached_request = std::move(request);
    GetBeforeLoadCache()->test_interface_requests.insert(cached_request);
  } else {
    test_html_viewers_.push_back(new TestHTMLViewerImpl(
        frame_->web_frame()->toWebLocalFrame(), std::move(request)));
  }
}

void HTMLDocument::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<web_view::mojom::FrameClient> request) {
  if (frame_) {
    DVLOG(1) << "Request for FrameClient after one already vended.";
    return;
  }
  resource_waiter_->Bind(std::move(request));
}

void HTMLDocument::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<devtools_service::DevToolsAgent> request) {
  if (frame_) {
    if (frame_->devtools_agent())
      frame_->devtools_agent()->BindToRequest(std::move(request));
  } else {
    devtools_agent_request_ = std::move(request);
  }
}

void HTMLDocument::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) {
  DCHECK(!transferable_state_.window_tree_delegate_impl);
  transferable_state_.window_tree_delegate_impl.reset(
      new WindowTreeDelegateImpl(this));
  transferable_state_.owns_window_tree_connection = true;
  mus::WindowTreeConnection::Create(
      transferable_state_.window_tree_delegate_impl.get(), std::move(request),
      mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

}  // namespace html_viewer
