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
#include "mandoline/ui/browser/browser_manager.h"
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

Browser::Browser(mojo::ApplicationImpl* app, BrowserManager* browser_manager)
    : view_manager_init_(app, this, this),
      root_(nullptr),
      content_(nullptr),
      omnibox_(nullptr),
      navigator_host_(this),
      app_(app),
      browser_manager_(browser_manager) {
  view_manager_init_.connection()->AddService<ViewEmbedder>(this);

  ui_.reset(BrowserUI::Create(this, app));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line->GetArgs();
  if (args.empty()) {
    default_url_ = "http://www.google.com/";
  } else {
#if defined(OS_WIN)
    default_url_ = base::WideToUTF8(args[0]);
#else
    default_url_ = args[0];
#endif
  }
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

void Browser::OnDevicePixelRatioAvailable() {
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
  } else if (!default_url_.empty()) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(default_url_);
    Embed(request.Pass());
  }
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

  if (!browser_manager_->InitUIIfNecessary(this, root_))
    return;  // We'll be called back from OnDevicePixelRatioAvailable().
  OnDevicePixelRatioAvailable();
}

void Browser::OnEmbedForDescendant(mojo::View* view,
                                   mojo::URLRequestPtr request,
                                   mojo::ViewManagerClientPtr* client) {
  // TODO(sky): move this to Frame/FrameTree.
  Frame* frame = Frame::FindFirstFrameAncestor(view);
  if (!frame) {
    // TODO(sky): add requestor url so that we can return false if it's not
    // an app we expect.
    mojo::ApplicationConnection* connection =
        app_->ConnectToApplication(request.Pass());
    connection->ConnectToService(client);
    return;
  }

  Frame* parent = frame;
  if (frame->view() == view) {
    parent = frame->parent();
    // This is a reembed.
    delete frame;
    frame = nullptr;
  }

  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  frame_connection->Init(app_, request.Pass(), client);
  FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
  frame_tree_->CreateAndAddFrame(view, parent, frame_tree_client,
                                 frame_connection.Pass());
}

void Browser::OnViewManagerDestroyed(mojo::ViewManager* view_manager) {
  ui_.reset();
  root_ = nullptr;
  browser_manager_->BrowserClosed(this);
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
  frame_tree_.reset(new FrameTree(content_, nullptr, frame_tree_client,
                                  frame_connection.Pass()));
  content_->Embed(view_manager_client.Pass());

  navigator_host_.RecordNavigation(gurl.spec());
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
    omnibox_->SetVisible(true);
    omnibox_->SetBounds(root_->bounds());
  }
  mojo::ViewManagerClientPtr view_manager_client;
  mojo::ApplicationConnection* connection =
      app_->ConnectToApplication(request.Pass());
  connection->ConnectToService(&view_manager_client);
  omnibox_->Embed(view_manager_client.Pass());
}

}  // namespace mandoline
