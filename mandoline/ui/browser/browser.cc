// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
#include "mandoline/tab/frame.h"
#include "mandoline/tab/frame_connection.h"
#include "mandoline/tab/frame_tree.h"
#include "mandoline/ui/browser/browser_delegate.h"
#include "mandoline/ui/browser/browser_ui.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/gfx/geometry/size.h"

namespace mandoline {
namespace {

gfx::Size GetInitialViewportSize() {
#if defined(OS_ANDROID)
  // Resize to match the Nexus 5 aspect ratio:
  return gfx::Size(320, 640);
#else
  return gfx::Size(1280, 800);
#endif
}

}  // namespace

Browser::Browser(mojo::ApplicationImpl* app,
                 BrowserDelegate* delegate,
                 const GURL& default_url)
    : view_manager_init_(app, this, this),
      root_(nullptr),
      content_(nullptr),
      omnibox_(nullptr),
      default_url_(default_url),
      navigator_host_(this),
      app_(app),
      delegate_(delegate) {
  ui_.reset(BrowserUI::Create(this, app));
}

Browser::~Browser() {
  // Destruct ui_ manually while |this| is alive and reset the pointer first.
  // This is to avoid a double delete when OnViewManagerDestroyed gets
  // called.
  ui_.reset();
}

void Browser::ReplaceContentWithRequest(mojo::URLRequestPtr request) {
  Embed(request.Pass());
}

mojo::ApplicationConnection* Browser::GetViewManagerConnectionForTesting() {
  return view_manager_init_.connection();
}

void Browser::NavigateExistingFrame(Frame* frame, mojo::URLRequestPtr request) {
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  mojo::ViewManagerClientPtr view_manager_client;
  frame_connection->Init(app_, request.Pass(), &view_manager_client);
  frame->view()->Embed(view_manager_client.Pass());
  FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
  frame_tree_->CreateOrReplaceFrame(frame, frame->view(), frame_tree_client,
                                    frame_connection.Pass());
}

void Browser::OnEmbed(mojo::View* root) {
  // Browser does not support being embedded more than once.
  CHECK(!root_);

  // Make it so we get OnWillEmbed() for any Embed()s done by other apps we
  // Embed().
  root->view_manager()->SetEmbedRoot();

  // TODO(beng): still unhappy with the fact that both this class & the UI class
  //             know so much about these views. Figure out how to shift more to
  //             the UI class.
  root_ = root;

  delegate_->InitUIIfNecessary(this, root_);

  content_ = root_->view_manager()->CreateView();
  ui_->Init(root_);

  view_manager_init_.view_manager_root()->SetViewportSize(
      mojo::Size::From(GetInitialViewportSize()));

  root_->AddChild(content_);
  content_->SetVisible(true);

  view_manager_init_.view_manager_root()->AddAccelerator(
      mojo::KEYBOARD_CODE_BROWSER_BACK, mojo::EVENT_FLAGS_NONE);

  // Now that we're ready, either load a pending url or the default url.
  if (pending_request_) {
    Embed(pending_request_.Pass());
  } else if (default_url_.is_valid()) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(default_url_.spec());
    Embed(request.Pass());
  }
}

void Browser::OnEmbedForDescendant(mojo::View* view,
                                   mojo::URLRequestPtr request,
                                   mojo::ViewManagerClientPtr* client) {
  // TODO(sky): move this to Frame/FrameTree.
  Frame* frame = Frame::FindFirstFrameAncestor(view);
  if (!frame || !frame->HasAncestor(frame_tree_->root())) {
    // TODO(sky): add requestor url so that we can return false if it's not
    // an app we expect.
    scoped_ptr<mojo::ApplicationConnection> connection =
        app_->ConnectToApplication(request.Pass());
    connection->ConnectToService(client);
    return;
  }

  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  frame_connection->Init(app_, request.Pass(), client);
  FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
  frame_tree_->CreateOrReplaceFrame(frame, view, frame_tree_client,
                                    frame_connection.Pass());
}

void Browser::OnViewManagerDestroyed(mojo::ViewManager* view_manager) {
  ui_.reset();
  root_ = nullptr;
  delegate_->BrowserClosed(this);
}

void Browser::OnAccelerator(mojo::EventPtr event) {
  DCHECK_EQ(mojo::KEYBOARD_CODE_BROWSER_BACK,
            event->key_data->windows_key_code);
  navigator_host_.RequestNavigateHistory(-1);
}

void Browser::OpenURL(const mojo::String& url) {
  omnibox_->SetVisible(false);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(url);
  ReplaceContentWithRequest(request.Pass());
}

void Browser::Embed(mojo::URLRequestPtr request) {
  const std::string string_url = request->url.To<std::string>();
  if (string_url == "mojo:omnibox") {
    ShowOmnibox(request.Pass());
    return;
  }

  // We can get Embed calls before we've actually been
  // embedded into the root view and content_ is created.
  // Just save the last url, we'll embed it when we're ready.
  if (!content_) {
    pending_request_ = request.Pass();
    return;
  }

  GURL gurl(string_url);
  bool changed = current_url_ != gurl;
  current_url_ = gurl;
  if (changed)
    ui_->OnURLChanged();

  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  mojo::ViewManagerClientPtr view_manager_client;
  frame_connection->Init(app_, request.Pass(), &view_manager_client);
  frame_connection->application_connection()->AddService<mojo::NavigatorHost>(
      this);
  FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
  frame_tree_.reset(new FrameTree(content_, this, frame_tree_client,
                                  frame_connection.Pass()));
  content_->Embed(view_manager_client.Pass());
  LoadingStateChanged(true);

  navigator_host_.RecordNavigation(gurl.spec());
}

bool Browser::CanPostMessageEventToFrame(const Frame* source,
                                         const Frame* target,
                                         HTMLMessageEvent* event) {
  return true;
}

void Browser::LoadingStateChanged(bool loading) {
  ui_->LoadingStateChanged(loading);
}

void Browser::ProgressChanged(double progress) {
  ui_->ProgressChanged(progress);
}

void Browser::Create(mojo::ApplicationConnection* connection,
                     mojo::InterfaceRequest<mojo::NavigatorHost> request) {
  navigator_host_.Bind(request.Pass());
}

void Browser::Create(mojo::ApplicationConnection* connection,
                     mojo::InterfaceRequest<ViewEmbedder> request) {
  view_embedder_bindings_.AddBinding(this, request.Pass());
}

void Browser::ShowOmnibox(mojo::URLRequestPtr request) {
  if (!omnibox_) {
    omnibox_ = root_->view_manager()->CreateView();
    root_->AddChild(omnibox_);
    omnibox_->SetBounds(root_->bounds());
  }
  mojo::ViewManagerClientPtr view_manager_client;
  scoped_ptr<mojo::ApplicationConnection> connection =
      app_->ConnectToApplication(request.Pass());
  connection->AddService<ViewEmbedder>(this);
  connection->ConnectToService(&view_manager_client);
  omnibox_->Embed(view_manager_client.Pass());
  omnibox_->SetVisible(true);
}

void Browser::RequestNavigate(Frame* source,
                              NavigationTargetType target_type,
                              Frame* target_frame,
                              mojo::URLRequestPtr request) {
  // TODO: this needs security checks.
  if (target_type == NAVIGATION_TARGET_TYPE_EXISTING_FRAME) {
    if (target_frame && target_frame != frame_tree_->root() &&
        target_frame->view()) {
      NavigateExistingFrame(target_frame, request.Pass());
      return;
    }
    DVLOG(1) << "RequestNavigate() targeted existing frame that doesn't exist.";
    return;
  }
  ReplaceContentWithRequest(request.Pass());
}

}  // namespace mandoline
