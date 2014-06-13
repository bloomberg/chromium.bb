// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "mojo/services/public/interfaces/launcher/launcher.mojom.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace mojo {
namespace examples {

class NodeView : public views::View {
 public:
  explicit NodeView(view_manager::ViewTreeNode* node) : node_(node) {
    // This class is provisional and assumes that the node has already been
    // added to a parent. I suspect we'll want to make an improved version of
    // this that lives in ui/views akin to NativeViewHost that properly
    // attaches/detaches when the view is.
    DCHECK(node->parent());
  }
  virtual ~NodeView() {}

  // Overridden from views::View:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    node_->SetBounds(ConvertRectToWidget(GetLocalBounds()));
  }

 private:
  view_manager::ViewTreeNode* node_;

  DISALLOW_COPY_AND_ASSIGN(NodeView);
};

class BrowserLayoutManager : public views::LayoutManager {
 public:
  BrowserLayoutManager() {}
  virtual ~BrowserLayoutManager() {}

 private:
  // Overridden from views::LayoutManager:
  virtual void Layout(views::View* host) OVERRIDE {
    // Browser view has two children:
    // 1. text input field.
    // 2. content view.
    DCHECK_EQ(2, host->child_count());
    views::View* text_field = host->child_at(0);
    gfx::Size ps = text_field->GetPreferredSize();
    text_field->SetBoundsRect(gfx::Rect(host->width(), ps.height()));
    views::View* content_area = host->child_at(1);
    content_area->SetBounds(0, text_field->bounds().bottom(), host->width(),
                            host->height() - text_field->bounds().bottom());
  }
  virtual gfx::Size GetPreferredSize(const views::View* host) const OVERRIDE {
    return gfx::Size();
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserLayoutManager);
};

// This is the basics of creating a views widget with a textfield.
// TODO: cleanup!
class Browser : public Application,
                public view_manager::ViewManagerDelegate,
                public views::TextfieldController,
                public InterfaceImpl<launcher::LauncherClient> {
 public:
  Browser() : view_manager_(NULL), view_(NULL), content_node_(NULL) {}

  virtual ~Browser() {
  }

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    views_init_.reset(new ViewsInit);
    view_manager::ViewManager::Create(this, this);
    ConnectTo("mojo:mojo_launcher", &launcher_);
    launcher_.set_client(this);
  }

  void CreateWidget(const gfx::Size& size) {
    views::Textfield* textfield = new views::Textfield;
    textfield->set_controller(this);

    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->AddChildView(textfield);
    widget_delegate->GetContentsView()->AddChildView(
        new NodeView(content_node_));
    widget_delegate->GetContentsView()->SetLayoutManager(
        new BrowserLayoutManager);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.native_widget = new NativeWidgetViewManager(widget, view_);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(size.width(), size.height());
    widget->Init(params);
    widget->Show();
    textfield->RequestFocus();
  }

  // view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::ViewTreeNode* root) OVERRIDE {
    // TODO: deal with OnRootAdded() being invoked multiple times.
    view_manager_ = view_manager;
    view_ = view_manager::View::Create(view_manager_);
    view_manager_->GetRoots().front()->SetActiveView(view_);

    content_node_ = view_manager::ViewTreeNode::Create(view_manager_);
    root->AddChild(content_node_);

    root->SetFocus();

    CreateWidget(root->bounds().size());
  }
  virtual void OnRootRemoved(view_manager::ViewManager* view_manager,
                             view_manager::ViewTreeNode* root) OVERRIDE {
  }

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      GURL url(sender->text());
      printf("User entered this URL: %s\n", url.spec().c_str());
      launcher_->Launch(url.spec());
    }
    return false;
  }

  // launcher::LauncherClient:
  virtual void OnLaunch(
      const String& handler_url,
      URLResponsePtr response,
      ScopedDataPipeConsumerHandle response_body_stream) OVERRIDE {
    content_node_->Embed(handler_url);

    launcher::LaunchablePtr launchable;
    ConnectTo(handler_url, &launchable);
    launchable->OnLaunch(response.Pass(),
                         response_body_stream.Pass(),
                         content_node_->id());
  }

  scoped_ptr<ViewsInit> views_init_;

  view_manager::ViewManager* view_manager_;
  view_manager::View* view_;
  view_manager::ViewTreeNode* content_node_;
  launcher::LauncherPtr launcher_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::Browser;
}

}  // namespace mojo
