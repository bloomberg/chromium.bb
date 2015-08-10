// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_document_oopif.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/devtools_service/public/cpp/switches.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/devtools_agent_impl.h"
#include "components/html_viewer/document_resource_waiter.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/test_html_viewer_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

using mojo::AxProvider;
using mojo::View;

namespace html_viewer {
namespace {

const char kEnableTestInterface[] = "enable-html-viewer-test-interface";

bool IsTestInterfaceEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTestInterface);
}

bool EnableRemoteDebugging() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      devtools_service::kRemoteDebuggingPort);
}

}  // namespace

HTMLDocumentOOPIF::BeforeLoadCache::BeforeLoadCache() {
}

HTMLDocumentOOPIF::BeforeLoadCache::~BeforeLoadCache() {
  STLDeleteElements(&ax_provider_requests);
  STLDeleteElements(&test_interface_requests);
}

HTMLDocumentOOPIF::HTMLDocumentOOPIF(mojo::ApplicationImpl* html_document_app,
                                     mojo::ApplicationConnection* connection,
                                     mojo::URLResponsePtr response,
                                     GlobalState* global_state,
                                     const DeleteCallback& delete_callback)
    : app_refcount_(html_document_app->app_lifetime_helper()
                        ->CreateAppRefCount()),
      html_document_app_(html_document_app),
      connection_(connection),
      view_manager_client_factory_(html_document_app->shell(), this),
      global_state_(global_state),
      frame_(nullptr),
      delete_callback_(delete_callback) {
  // TODO(sky): nuke headless. We're not going to care about it anymore.
  DCHECK(!global_state_->is_headless());

  connection->AddService(
      static_cast<mojo::InterfaceFactory<mandoline::FrameTreeClient>*>(this));
  connection->AddService(static_cast<InterfaceFactory<AxProvider>*>(this));
  connection->AddService(&view_manager_client_factory_);
  if (IsTestInterfaceEnabled()) {
    connection->AddService(
        static_cast<mojo::InterfaceFactory<TestHTMLViewer>*>(this));
  }

  resource_waiter_.reset(
      new DocumentResourceWaiter(global_state_, response.Pass(), this));
  LoadIfNecessary();
}

void HTMLDocumentOOPIF::Destroy() {
  if (resource_waiter_) {
    mojo::View* root = resource_waiter_->root();
    if (root) {
      root->RemoveObserver(this);
      resource_waiter_.reset();
      delete root->view_manager();
    } else {
      delete this;
    }
  } else {
    // Closing the frame ends up destroying the ViewManager, which triggers
    // deleting this (OnViewManagerDestroyed()).
    frame_->Close();
  }
}

HTMLDocumentOOPIF::~HTMLDocumentOOPIF() {
  delete_callback_.Run(this);

  STLDeleteElements(&ax_providers_);
}

void HTMLDocumentOOPIF::LoadIfNecessary() {
  if (!frame_ && resource_waiter_->IsReady())
    Load();
}

void HTMLDocumentOOPIF::Load() {
  DCHECK(resource_waiter_ && resource_waiter_->IsReady());

  mojo::View* view = resource_waiter_->root();
  global_state_->InitIfNecessary(
      view->viewport_metrics().size_in_pixels.To<gfx::Size>(),
      view->viewport_metrics().device_pixel_ratio);

  view->RemoveObserver(this);

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response =
      resource_waiter_->ReleaseURLResponse().Pass();

  frame_ = HTMLFrameTreeManager::CreateFrameAndAttachToTree(
      global_state_, html_document_app_, view, resource_waiter_.Pass(), this);

  // TODO(yzshen): http://crbug.com/498986 Creating DevToolsAgentImpl instances
  // causes html_viewer_apptests flakiness currently. Before we fix that we
  // cannot enable remote debugging (which is required by Telemetry tests) on
  // the bots.
  if (EnableRemoteDebugging() && !frame_->parent()) {
    devtools_agent_.reset(new DevToolsAgentImpl(
        frame_->web_frame()->toWebLocalFrame(), html_document_app_->shell()));
  }

  const GURL url(extra_data->synthetic_response->url);

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  frame_->web_frame()->toWebLocalFrame()->loadRequest(web_request);
}

HTMLDocumentOOPIF::BeforeLoadCache* HTMLDocumentOOPIF::GetBeforeLoadCache() {
  CHECK(!did_finish_local_frame_load_);
  if (!before_load_cache_.get())
    before_load_cache_.reset(new BeforeLoadCache);
  return before_load_cache_.get();
}

void HTMLDocumentOOPIF::OnEmbed(View* root) {
  // We're an observer until the document is loaded.
  root->AddObserver(this);
  resource_waiter_->set_root(root);

  LoadIfNecessary();
}

void HTMLDocumentOOPIF::OnUnembed() {
  frame_->OnViewUnembed();
}

void HTMLDocumentOOPIF::OnViewManagerDestroyed(
    mojo::ViewManager* view_manager) {
  delete this;
}

void HTMLDocumentOOPIF::OnViewViewportMetricsChanged(
    mojo::View* view,
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  LoadIfNecessary();
}

void HTMLDocumentOOPIF::OnViewDestroyed(View* view) {
  resource_waiter_->root()->RemoveObserver(this);
  resource_waiter_->set_root(nullptr);
}

bool HTMLDocumentOOPIF::ShouldNavigateLocallyInMainFrame() {
  return devtools_agent_ && devtools_agent_->handling_page_navigate_request();
}

void HTMLDocumentOOPIF::OnFrameDidFinishLoad() {
  did_finish_local_frame_load_ = true;
  scoped_ptr<BeforeLoadCache> before_load_cache = before_load_cache_.Pass();
  if (!before_load_cache)
    return;

  // Bind any pending AxProvider and TestHTMLViewer interface requests.
  for (auto it : before_load_cache->ax_provider_requests) {
    ax_providers_.insert(new AxProviderImpl(
        frame_->frame_tree_manager()->GetWebView(), it->Pass()));
  }
  for (auto it : before_load_cache->test_interface_requests) {
    CHECK(IsTestInterfaceEnabled());
    test_html_viewers_.push_back(new TestHTMLViewerImpl(
        frame_->web_frame()->toWebLocalFrame(), it->Pass()));
  }
}

mojo::ApplicationImpl* HTMLDocumentOOPIF::GetApp() {
  return html_document_app_;
}

void HTMLDocumentOOPIF::Create(mojo::ApplicationConnection* connection,
                               mojo::InterfaceRequest<AxProvider> request) {
  if (!did_finish_local_frame_load_) {
    // Cache AxProvider interface requests until the document finishes loading.
    auto cached_request = new mojo::InterfaceRequest<AxProvider>();
    *cached_request = request.Pass();
    GetBeforeLoadCache()->ax_provider_requests.insert(cached_request);
  } else {
    ax_providers_.insert(
        new AxProviderImpl(frame_->web_view(), request.Pass()));
  }
}

void HTMLDocumentOOPIF::Create(mojo::ApplicationConnection* connection,
                               mojo::InterfaceRequest<TestHTMLViewer> request) {
  CHECK(IsTestInterfaceEnabled());
  if (!did_finish_local_frame_load_) {
    auto cached_request = new mojo::InterfaceRequest<TestHTMLViewer>();
    *cached_request = request.Pass();
    GetBeforeLoadCache()->test_interface_requests.insert(cached_request);
  } else {
    test_html_viewers_.push_back(new TestHTMLViewerImpl(
        frame_->web_frame()->toWebLocalFrame(), request.Pass()));
  }
}

void HTMLDocumentOOPIF::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mandoline::FrameTreeClient> request) {
  if (frame_) {
    DVLOG(1) << "Request for FrameTreeClient after one already vended.";
    return;
  }
  resource_waiter_->Bind(request.Pass());
}

}  // namespace html_viewer
