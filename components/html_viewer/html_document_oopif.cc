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
#include "components/html_viewer/frame.h"
#include "components/html_viewer/frame_tree_manager.h"
#include "components/html_viewer/global_state.h"
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

bool EnableRemoteDebugging() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      devtools_service::kRemoteDebuggingPort);
}

}  // namespace

HTMLDocumentOOPIF::HTMLDocumentOOPIF(mojo::ApplicationImpl* html_document_app,
                                     mojo::ApplicationConnection* connection,
                                     mojo::URLResponsePtr response,
                                     GlobalState* global_state,
                                     const DeleteCallback& delete_callback)
    : app_refcount_(
          html_document_app->app_lifetime_helper()->CreateAppRefCount()),
      html_document_app_(html_document_app),
      connection_(connection),
      view_manager_client_factory_(html_document_app->shell(), this),
      global_state_(global_state),
      delete_callback_(delete_callback) {
  // TODO(sky): nuke headless. We're not going to care about it anymore.
  DCHECK(!global_state_->is_headless());

  connection->AddService(
      static_cast<mojo::InterfaceFactory<mandoline::FrameTreeClient>*>(this));
  connection->AddService(static_cast<InterfaceFactory<AxProvider>*>(this));
  connection->AddService(&view_manager_client_factory_);

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
    DCHECK(frame_tree_manager_);
    mojo::ViewManager* view_manager =
        frame_tree_manager_->GetLocalFrame()->view()->view_manager();
    frame_tree_manager_.reset();

    // Delete the ViewManager, which will trigger deleting us.
    delete view_manager;
  }
}

HTMLDocumentOOPIF::~HTMLDocumentOOPIF() {
  delete_callback_.Run(this);

  STLDeleteElements(&ax_providers_);
  STLDeleteElements(&ax_provider_requests_);
}

void HTMLDocumentOOPIF::LoadIfNecessary() {
  if (!frame_tree_manager_ && resource_waiter_->IsReady())
    Load();
}

void HTMLDocumentOOPIF::Load() {
  DCHECK(resource_waiter_ && resource_waiter_->IsReady());

  mojo::View* view = resource_waiter_->root();
  global_state_->InitIfNecessary(
      view->viewport_metrics().size_in_pixels.To<gfx::Size>(),
      view->viewport_metrics().device_pixel_ratio);

  mojo::InterfaceRequest<mandoline::FrameTreeClient> frame_tree_client_request;
  mandoline::FrameTreeServerPtr frame_tree_server;
  mojo::Array<mandoline::FrameDataPtr> frame_data;
  mojo::URLResponsePtr response;
  resource_waiter_->Release(&frame_tree_client_request, &frame_tree_server,
                            &frame_data, &response);
  resource_waiter_.reset();

  view->RemoveObserver(this);

  frame_tree_manager_.reset(
      new FrameTreeManager(global_state_, html_document_app_, connection_,
                           view->id(), frame_tree_server.Pass()));
  frame_tree_manager_->set_delegate(this);
  frame_tree_manager_binding_.reset(
      new mojo::Binding<mandoline::FrameTreeClient>(
          frame_tree_manager_.get(), frame_tree_client_request.Pass()));
  frame_tree_manager_->Init(view, frame_data.Pass());

  // TODO(yzshen): http://crbug.com/498986 Creating DevToolsAgentImpl instances
  // causes html_viewer_apptests flakiness currently. Before we fix that we
  // cannot enable remote debugging (which is required by Telemetry tests) on
  // the bots.
  if (EnableRemoteDebugging()) {
    Frame* frame = frame_tree_manager_->GetLocalFrame();
    if (!frame->parent()) {
      devtools_agent_.reset(new DevToolsAgentImpl(
          frame->web_frame()->toWebLocalFrame(), html_document_app_->shell()));
    }
  }

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response = response.Pass();

  const GURL url(extra_data->synthetic_response->url);

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  frame_tree_manager_->GetLocalWebFrame()->loadRequest(web_request);
}

void HTMLDocumentOOPIF::OnEmbed(View* root) {
  // We're an observer until the document is loaded.
  root->AddObserver(this);
  resource_waiter_->set_root(root);

  LoadIfNecessary();
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

void HTMLDocumentOOPIF::OnFrameDidFinishLoad(Frame* frame) {
  // TODO(msw): Notify AxProvider clients of updates on child frame loads.
  if (frame != frame_tree_manager_->GetLocalFrame())
    return;

  did_finish_main_frame_load_ = true;

  // Bind any pending AxProviderImpl interface requests.
  for (auto it : ax_provider_requests_)
    ax_providers_.insert(new AxProviderImpl(frame->web_view(), it->Pass()));
  STLDeleteElements(&ax_provider_requests_);
}

void HTMLDocumentOOPIF::Create(mojo::ApplicationConnection* connection,
                               mojo::InterfaceRequest<AxProvider> request) {
  if (!did_finish_main_frame_load_) {
    // Cache AxProvider interface requests until the document finishes loading.
    auto cached_request = new mojo::InterfaceRequest<AxProvider>();
    *cached_request = request.Pass();
    ax_provider_requests_.insert(cached_request);
  } else {
    ax_providers_.insert(new AxProviderImpl(
        frame_tree_manager_->GetLocalFrame()->web_view(), request.Pass()));
  }
}

void HTMLDocumentOOPIF::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mandoline::FrameTreeClient> request) {
  if (frame_tree_manager_.get() || frame_tree_manager_binding_.get()) {
    DVLOG(1) << "Request for FrameTreeClient after one already vended.";
    return;
  }
  resource_waiter_->Bind(request.Pass());
}

}  // namespace html_viewer
