// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/aura/image_window_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

// A LayerDelegate that paints an image for the layer.
class ImageLayerDelegate : public ui::LayerDelegate {
 public:
  ImageLayerDelegate() {}

  virtual ~ImageLayerDelegate() {}

  void SetImage(const gfx::Image& image) {
    image_ = image;
    image_size_ = image.AsImageSkia().size();
  }
  const gfx::Image& image() const { return image_; }

 private:
  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    if (image_.IsEmpty()) {
      canvas->DrawColor(SK_ColorGRAY);
    } else {
      SkISize size = canvas->sk_canvas()->getDeviceSize();
      if (size.width() != image_size_.width() ||
          size.height() != image_size_.height()) {
        canvas->DrawColor(SK_ColorWHITE);
      }
      canvas->DrawImageInt(image_.AsImageSkia(), 0, 0);
    }
  }

  // Called when the layer's device scale factor has changed.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  // Invoked prior to the bounds changing. The returned closured is run after
  // the bounds change.
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  gfx::Image image_;
  gfx::Size image_size_;

  DISALLOW_COPY_AND_ASSIGN(ImageLayerDelegate);
};

OverscrollNavigationOverlay::OverscrollNavigationOverlay(
    WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      image_delegate_(NULL),
      loading_complete_(false),
      received_paint_update_(false),
      slide_direction_(SLIDE_UNKNOWN),
      need_paint_update_(true) {
}

OverscrollNavigationOverlay::~OverscrollNavigationOverlay() {
}

void OverscrollNavigationOverlay::StartObserving() {
  loading_complete_ = false;
  received_paint_update_ = false;
  Observe(web_contents_);

  // Make sure the overlay window is on top.
  if (window_.get() && window_->parent())
    window_->parent()->StackChildAtTop(window_.get());
}

void OverscrollNavigationOverlay::SetOverlayWindow(
    scoped_ptr<aura::Window> window,
    ImageWindowDelegate* delegate) {
  window_ = window.Pass();
  if (window_.get() && window_->parent())
    window_->parent()->StackChildAtTop(window_.get());
  image_delegate_ = delegate;

  if (window_.get() && delegate->has_image()) {
    window_slider_.reset(new WindowSlider(this,
                                          window_->parent(),
                                          window_.get()));
    slide_direction_ = SLIDE_UNKNOWN;
  } else {
    window_slider_.reset();
  }
}

void OverscrollNavigationOverlay::SetupForTesting() {
  need_paint_update_ = false;
}

void OverscrollNavigationOverlay::StopObservingIfDone() {
  // If there is a screenshot displayed in the overlay window, then wait for
  // the navigated page to complete loading and some paint update before
  // hiding the overlay.
  // If there is no screenshot in the overlay window, then hide this view
  // as soon as there is any new painting notification.
  if ((need_paint_update_ && !received_paint_update_) ||
      (image_delegate_->has_image() && !loading_complete_)) {
    return;
  }

  // If a slide is in progress, then do not destroy the window or the slide.
  if (window_slider_.get() && window_slider_->IsSlideInProgress())
    return;

  Observe(NULL);
  window_slider_.reset();
  window_.reset();
  image_delegate_ = NULL;
}

ui::Layer* OverscrollNavigationOverlay::CreateSlideLayer(int offset) {
  const NavigationControllerImpl& controller = web_contents_->GetController();
  const NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
      controller.GetEntryAtOffset(offset));

  gfx::Image image;
  if (entry && entry->screenshot().get()) {
    std::vector<gfx::ImagePNGRep> image_reps;
    image_reps.push_back(gfx::ImagePNGRep(entry->screenshot(),
        ui::GetImageScale(
            ui::GetScaleFactorForNativeView(window_.get()))));
    image = gfx::Image(image_reps);
  }
  if (!layer_delegate_)
    layer_delegate_.reset(new ImageLayerDelegate());
  layer_delegate_->SetImage(image);

  ui::Layer* layer = new ui::Layer(ui::LAYER_TEXTURED);
  layer->set_delegate(layer_delegate_.get());
  return layer;
}

void OverscrollNavigationOverlay::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  if (loading_complete_ &&
      ViewHostMsg_UpdateRect_Flags::is_repaint_ack(params.flags)) {
    // This is a paint update after the page has been loaded. So do not wait for
    // a 'first non-empty' paint update.
    received_paint_update_ = true;
    StopObservingIfDone();
  }
}

ui::Layer* OverscrollNavigationOverlay::CreateBackLayer() {
  if (!web_contents_->GetController().CanGoBack())
    return NULL;
  slide_direction_ = SLIDE_BACK;
  return CreateSlideLayer(-1);
}

ui::Layer* OverscrollNavigationOverlay::CreateFrontLayer() {
  if (!web_contents_->GetController().CanGoForward())
    return NULL;
  slide_direction_ = SLIDE_FRONT;
  return CreateSlideLayer(1);
}

void OverscrollNavigationOverlay::OnWindowSlideComplete() {
  if (slide_direction_ == SLIDE_UNKNOWN) {
    window_slider_.reset();
    StopObservingIfDone();
    return;
  }

  // Change the image used for the overlay window.
  image_delegate_->SetImage(layer_delegate_->image());
  window_->layer()->SetTransform(gfx::Transform());
  window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));

  SlideDirection direction = slide_direction_;
  slide_direction_ = SLIDE_UNKNOWN;

  // Reset state and wait for the new navigation page to complete
  // loading/painting.
  StartObserving();

  // Perform the navigation.
  if (direction == SLIDE_BACK)
    web_contents_->GetController().GoBack();
  else if (direction == SLIDE_FRONT)
    web_contents_->GetController().GoForward();
  else
    NOTREACHED();
}

void OverscrollNavigationOverlay::OnWindowSlideAborted() {
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::OnWindowSliderDestroyed() {
  // The slider has just been destroyed. Release the ownership.
  WindowSlider* slider ALLOW_UNUSED = window_slider_.release();
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::DidFirstVisuallyNonEmptyPaint(int32 page_id) {
  received_paint_update_ = true;
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::DidStopLoading(RenderViewHost* host) {
  loading_complete_ = true;
  if (!received_paint_update_) {
    // Force a repaint after the page is loaded.
    RenderViewHostImpl* view = static_cast<RenderViewHostImpl*>(host);
    view->ScheduleComposite();
  }
  StopObservingIfDone();
}

bool OverscrollNavigationOverlay::OnMessageReceived(
    const IPC::Message& message) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  IPC_BEGIN_MESSAGE_MAP(OverscrollNavigationOverlay, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
  IPC_END_MESSAGE_MAP()
  return false;
}

}  // namespace content
