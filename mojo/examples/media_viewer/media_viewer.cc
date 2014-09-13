// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/examples/media_viewer/media_viewer.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
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

  void Initialize(View* view) {
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
    params.native_widget = new NativeWidgetViewManager(widget, view);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(view->bounds().width(), view->bounds().height());
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

class MediaViewer
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ControlPanel::Delegate,
      public ViewObserver {
 public:
  MediaViewer()
      : app_(NULL),
        view_manager_(NULL),
        root_view_(NULL),
        control_view_(NULL),
        content_view_(NULL),
        control_panel_(this) {
    handler_map_["image/png"] = "mojo:mojo_png_viewer";
  }

  virtual ~MediaViewer() {
    if (root_view_)
      root_view_->RemoveObserver(this);
  }

 private:
  typedef std::map<std::string, std::string> HandlerMap;


  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
    app_ = app;
    views_init_.reset(new ViewsInit);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      OVERRIDE {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  void LayoutViews() {
    View* root = content_view_->parent();
    gfx::Rect control_bounds(root->bounds().width(), 28);
    control_view_->SetBounds(control_bounds);
    gfx::Rect content_bounds(0, control_bounds.height(), root->bounds().width(),
                             root->bounds().height() - control_bounds.height());
    content_view_->SetBounds(content_bounds);
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    root_view_ = root;
    view_manager_ = view_manager;

    control_view_ = View::Create(view_manager_);
    root_view_->AddChild(control_view_);

    content_view_ = View::Create(view_manager_);
    root_view_->AddChild(content_view_);

    control_panel_.Initialize(control_view_);

    LayoutViews();
    root_view_->AddObserver(this);

    content_view_->Embed("TODO");
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

  // ViewObserver:
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    LayoutViews();
  }
  virtual void OnViewDestroyed(View* view) OVERRIDE {
    DCHECK_EQ(view, root_view_);
    view->RemoveObserver(this);
    root_view_ = NULL;
  }

  std::string GetHandlerForContentType(const std::string& content_type) {
    HandlerMap::const_iterator it = handler_map_.find(content_type);
    return it != handler_map_.end() ? it->second : std::string();
  }

  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  ApplicationImpl* app_;
  scoped_ptr<ViewsInit> views_init_;
  ViewManager* view_manager_;
  View* root_view_;
  View* control_view_;
  View* content_view_;
  ControlPanel control_panel_;
  ZoomableMediaPtr zoomable_media_;
  HandlerMap handler_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaViewer);
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::MediaViewer);
  return runner.Run(shell_handle);
}
