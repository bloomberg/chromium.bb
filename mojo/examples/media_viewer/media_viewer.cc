// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/examples/media_viewer/media_viewer.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace mojo {
namespace examples {

class MediaViewer;

class CustomButtonBorder: public views::Border {
 public:
  CustomButtonBorder()
      : normal_painter_(CreatePainter(SkColorSetRGB(0x80, 0x80, 0x80),
                                      SkColorSetRGB(0xC0, 0xC0, 0xC0))),
        hot_painter_(CreatePainter(SkColorSetRGB(0xA0, 0xA0, 0xA0),
                                   SkColorSetRGB(0xD0, 0xD0, 0xD0))),
        pushed_painter_(CreatePainter(SkColorSetRGB(0x80, 0x80, 0x80),
                                      SkColorSetRGB(0x90, 0x90, 0x90))),
        insets_(2, 6, 2, 6) {
  }
  virtual ~CustomButtonBorder() {}

 private:
  // Overridden from views::Border:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE {
    const views::LabelButton* button =
        static_cast<const views::LabelButton*>(&view);
    views::Button::ButtonState state = button->state();

    views::Painter* painter = normal_painter_.get();
    if (state == views::Button::STATE_HOVERED) {
      painter = hot_painter_.get();
    } else if (state == views::Button::STATE_PRESSED) {
      painter = pushed_painter_.get();
    }
    painter->Paint(canvas, view.size());
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return insets_;
  }

  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    gfx::Size size;
    if (normal_painter_)
      size.SetToMax(normal_painter_->GetMinimumSize());
    if (hot_painter_)
      size.SetToMax(hot_painter_->GetMinimumSize());
    if (pushed_painter_)
      size.SetToMax(pushed_painter_->GetMinimumSize());
    return size;
  }

  scoped_ptr<views::Painter> CreatePainter(SkColor border, SkColor background) {
    skia::RefPtr<SkCanvas> canvas(skia::AdoptRef(skia::CreatePlatformCanvas(
        64, 64, false)));
    SkPaint paint;
    paint.setColor(background);
    canvas->drawRoundRect(SkRect::MakeWH(63, 63), 2, 2, paint);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(border);
    canvas->drawRoundRect(SkRect::MakeWH(63, 63), 2, 2, paint);

    return scoped_ptr<views::Painter>(
        views::Painter::CreateImagePainter(
            gfx::ImageSkia::CreateFrom1xBitmap(
                skia::GetTopDevice(*canvas)->accessBitmap(true)),
            gfx::Insets(5, 5, 5, 5)));
  }

  scoped_ptr<views::Painter> normal_painter_;
  scoped_ptr<views::Painter> hot_painter_;
  scoped_ptr<views::Painter> pushed_painter_;

  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(CustomButtonBorder);
};

class ControlPanel : public views::ButtonListener {
 public:
  enum ControlType {
    CONTROL_ZOOM_IN,
    CONTROL_ACTUAL_SIZE,
    CONTROL_ZOOM_OUT,
    CONTROL_COUNT,
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void ButtonPressed(ControlType type) = 0;
  };

  ControlPanel(Delegate* delegate) : delegate_(delegate), buttons_() {}

  virtual ~ControlPanel() {}

  void Initialize(Node* node) {
    const char* kNames[] = { "Zoom In", "Actual Size", "Zoom Out" };

    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;

    widget_delegate->GetContentsView()->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 5, 2, 5));

    widget_delegate->GetContentsView()->set_background(
        views::Background::CreateSolidBackground(SK_ColorLTGRAY));

    for (int type = 0; type < CONTROL_COUNT; ++type) {
      views::Button* button = new views::LabelButton(
          this, base::ASCIIToUTF16(kNames[type]));
      button->SetBorder(scoped_ptr<views::Border>(new CustomButtonBorder));
      buttons_[type] = button;
      widget_delegate->GetContentsView()->AddChildView(button);
    }

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.native_widget = new NativeWidgetViewManager(widget, node);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(node->bounds().width(), node->bounds().height());
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
    widget->Init(params);
    widget->Show();
  }

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    for (int i = 0; i < CONTROL_COUNT; ++i) {
      if (sender == buttons_[i]) {
        delegate_->ButtonPressed(static_cast<ControlType>(i));
        return;
      }
    }
  }

  Delegate* delegate_;
  views::Button* buttons_[CONTROL_COUNT];

  DISALLOW_COPY_AND_ASSIGN(ControlPanel);
};

class NavigatorImpl : public InterfaceImpl<Navigator> {
 public:
  explicit NavigatorImpl(MediaViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from Navigator:
  virtual void Navigate(
      uint32_t node_id,
      NavigationDetailsPtr navigation_details,
      ResponseDetailsPtr response_details) OVERRIDE;

  MediaViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class MediaViewer
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ControlPanel::Delegate,
      public NodeObserver {
 public:
  MediaViewer()
      : navigator_factory_(this),
        view_manager_client_factory_(this),
        app_(NULL),
        view_manager_(NULL),
        root_node_(NULL),
        control_node_(NULL),
        content_node_(NULL),
        control_panel_(this) {
    handler_map_["image/png"] = "mojo:mojo_png_viewer";
  }

  virtual ~MediaViewer() {
    if (root_node_)
      root_node_->RemoveObserver(this);
  }

  void Navigate(
      uint32_t node_id,
      NavigationDetailsPtr navigation_details,
      ResponseDetailsPtr response_details) {
    // TODO(yzshen): This shouldn't be needed once FIFO is ready.
    if (!view_manager_) {
      pending_navigate_request_.reset(new PendingNavigateRequest);
      pending_navigate_request_->node_id = node_id;
      pending_navigate_request_->navigation_details = navigation_details.Pass();
      pending_navigate_request_->response_details = response_details.Pass();

      return;
    }

    std::string handler = GetHandlerForContentType(
        response_details->response->mime_type);
    if (handler.empty())
      return;

    content_node_->Embed(handler);

    if (navigation_details) {
      NavigatorPtr navigator;
      app_->ConnectToService(handler, &navigator);
      navigator->Navigate(content_node_->id(), navigation_details.Pass(),
                          response_details.Pass());
    }

    // TODO(yzshen): determine the set of controls to show based on what
    // interfaces the embedded app provides.
    app_->ConnectToService(handler, &zoomable_media_);
  }

 private:
  typedef std::map<std::string, std::string> HandlerMap;

  struct PendingNavigateRequest {
    uint32_t node_id;
    NavigationDetailsPtr navigation_details;
    ResponseDetailsPtr response_details;
  };


  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    app_ = app;
    views_init_.reset(new ViewsInit);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      OVERRIDE {
    connection->AddService(&navigator_factory_);
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  void LayoutNodes() {
    Node* root = content_node_->parent();
    gfx::Rect control_bounds(root->bounds().width(), 28);
    control_node_->SetBounds(control_bounds);
    gfx::Rect content_bounds(0, control_bounds.height(), root->bounds().width(),
                             root->bounds().height() - control_bounds.height());
    content_node_->SetBounds(content_bounds);
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       Node* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    root_node_ = root;
    view_manager_ = view_manager;

    control_node_ = Node::Create(view_manager_);
    root_node_->AddChild(control_node_);

    content_node_ = Node::Create(view_manager_);
    root_node_->AddChild(content_node_);

    control_node_->SetActiveView(View::Create(view_manager_));
    control_panel_.Initialize(control_node_);

    LayoutNodes();
    root_node_->AddObserver(this);

    if (pending_navigate_request_) {
      scoped_ptr<PendingNavigateRequest> request(
          pending_navigate_request_.release());

      Navigate(request->node_id, request->navigation_details.Pass(),
               request->response_details.Pass());
    }
  }
  virtual void OnViewManagerDisconnected(
      ViewManager* view_manager) OVERRIDE {
    DCHECK_EQ(view_manager_, view_manager);
    view_manager_ = NULL;
    base::MessageLoop::current()->Quit();
  }

  // Overridden from ControlPanel::Delegate:
  virtual void ButtonPressed(ControlPanel::ControlType type) OVERRIDE {
    switch (type) {
      case ControlPanel::CONTROL_ZOOM_IN:
        zoomable_media_->ZoomIn();
        break;
      case ControlPanel::CONTROL_ACTUAL_SIZE:
       zoomable_media_->ZoomToActualSize();
        break;
      case ControlPanel::CONTROL_ZOOM_OUT:
        zoomable_media_->ZoomOut();
        break;
      default:
        NOTIMPLEMENTED();
    }
  }

  // NodeObserver:
  virtual void OnNodeBoundsChanged(Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    LayoutNodes();
  }
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    DCHECK_EQ(node, root_node_);
    node->RemoveObserver(this);
    root_node_ = NULL;
  }

  std::string GetHandlerForContentType(const std::string& content_type) {
    HandlerMap::const_iterator it = handler_map_.find(content_type);
    return it != handler_map_.end() ? it->second : std::string();
  }

  InterfaceFactoryImplWithContext<NavigatorImpl, MediaViewer>
      navigator_factory_;
  ViewManagerClientFactory view_manager_client_factory_;

  ApplicationImpl* app_;
  scoped_ptr<ViewsInit> views_init_;
  ViewManager* view_manager_;
  Node* root_node_;
  Node* control_node_;
  Node* content_node_;
  ControlPanel control_panel_;
  ZoomableMediaPtr zoomable_media_;
  HandlerMap handler_map_;
  scoped_ptr<PendingNavigateRequest> pending_navigate_request_;

  DISALLOW_COPY_AND_ASSIGN(MediaViewer);
};

void NavigatorImpl::Navigate(
    uint32_t node_id,
    NavigationDetailsPtr navigation_details,
    ResponseDetailsPtr response_details) {
  viewer_->Navigate(node_id, navigation_details.Pass(),
                    response_details.Pass());
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::MediaViewer;
}

}  // namespace mojo
