// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_view.h"

#include <algorithm>
#include <utility>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/deep_link_util.h"
#include "base/callback.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// ContentsMaskPainter ---------------------------------------------------------

class ContentsMaskPainter : public views::Painter {
 public:
  ContentsMaskPainter() = default;
  ~ContentsMaskPainter() override = default;

  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorBLACK);

    SkRRect rect;
    rect.setRectRadii(
        SkRect::MakeWH(size.width(), size.height()),
        (const SkVector[]){
            /*upper_left=*/SkVector::Make(0, 0),
            /*upper_right=*/SkVector::Make(0, 0),
            /*lower_right=*/SkVector::Make(kCornerRadiusDip, kCornerRadiusDip),
            /*lower_left=*/SkVector::Make(kCornerRadiusDip, kCornerRadiusDip)});

    canvas->sk_canvas()->drawRRect(rect, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentsMaskPainter);
};

}  // namespace

// AssistantWebView ------------------------------------------------------------

AssistantWebView::AssistantWebView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), weak_factory_(this) {
  InitLayout();

  assistant_controller_->AddObserver(this);
}

AssistantWebView::~AssistantWebView() {
  assistant_controller_->RemoveObserver(this);

  RemoveContents();
}

const char* AssistantWebView::GetClassName() const {
  return "AssistantWebView";
}

gfx::Size AssistantWebView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantWebView::GetHeightForWidth(int width) const {
  // |height| <= |kMaxHeightDip|.
  // |height| should not exceed the height of the usable work area.
  gfx::Rect usable_work_area =
      assistant_controller_->ui_controller()->model()->usable_work_area();

  return std::min(kMaxHeightDip, usable_work_area.height());
}

void AssistantWebView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebView has a fixed size, it does not re-layout its
  // children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  Layout();
  SchedulePaint();
}

void AssistantWebView::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  // The mask layer should always match the bounds of the contents' native view.
  contents_mask_->layer()->SetBounds(gfx::Rect(new_bounds.size()));
}

void AssistantWebView::OnWindowDestroying(aura::Window* window) {
  // It's possible for |window| to be deleted before AssistantWebView. When this
  // happens, we need to perform clean up on |window| before it is destroyed.
  window->RemoveObserver(this);
  window->layer()->SetMaskLayer(nullptr);
}

void AssistantWebView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Caption bar.
  caption_bar_ = new CaptionBar();
  caption_bar_->set_delegate(this);
  caption_bar_->SetButtonVisible(AssistantButtonId::kMinimize, false);
  AddChildView(caption_bar_);

  // Contents mask.
  // This is used to enforce corner radius on the contents' native view layer.
  contents_mask_ = views::Painter::CreatePaintedLayer(
      std::make_unique<ContentsMaskPainter>());
  contents_mask_->layer()->SetFillsBoundsOpaquely(false);
}

bool AssistantWebView::OnCaptionButtonPressed(AssistantButtonId id) {
  // We need special handling of the back button. When possible, the back button
  // should navigate backwards in the web contents' history stack.
  if (id == AssistantButtonId::kBack && contents_) {
    contents_->GoBack(base::BindOnce(
        [](const base::WeakPtr<AssistantWebView>& assistant_web_view,
           bool success) {
          // If we can't navigate back in the web contents' history stack we
          // defer back to our primary caption button delegate.
          if (!success && assistant_web_view) {
            assistant_web_view->assistant_controller_->ui_controller()
                ->OnCaptionButtonPressed(AssistantButtonId::kBack);
          }
        },
        weak_factory_.GetWeakPtr()));
    return true;
  }

  // For all other buttons we defer to our primary caption button delegate.
  return assistant_controller_->ui_controller()->OnCaptionButtonPressed(id);
}

void AssistantWebView::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type))
    return;

  RemoveContents();

  assistant_controller_->GetNavigableContentsFactory(
      mojo::MakeRequest(&contents_factory_));

  const gfx::Size preferred_size =
      gfx::Size(kPreferredWidthDip,
                kMaxHeightDip - caption_bar_->GetPreferredSize().height());

  auto contents_params = content::mojom::NavigableContentsParams::New();
  contents_params->enable_view_auto_resize = true;
  contents_params->auto_resize_min_size = preferred_size;
  contents_params->auto_resize_max_size = preferred_size;
  contents_params->suppress_navigations = true;

  contents_ = std::make_unique<content::NavigableContents>(
      contents_factory_.get(), std::move(contents_params));

  // We observe |contents_| so that we can handle events from the underlying
  // web contents.
  contents_->AddObserver(this);

  // Navigate to the url associated with the received deep link.
  contents_->Navigate(assistant::util::GetWebUrl(type, params).value());
}

void AssistantWebView::DidAutoResizeView(const gfx::Size& new_size) {
  contents_->GetView()->view()->SetPreferredSize(new_size);
}

void AssistantWebView::DidStopLoading() {
  AddChildView(contents_->GetView()->view());

  gfx::NativeView native_view = contents_->GetView()->native_view();

  // We apply a layer mask to the contents' native view to enforce our desired
  // corner radius. We need to sync the bounds of mask layer with the bounds
  // of the native view prior to application to prevent DCHECK failure.
  contents_mask_->layer()->SetBounds(
      gfx::Rect(native_view->GetBoundsInScreen().size()));
  native_view->layer()->SetMaskLayer(contents_mask_->layer());

  // We observe |native_view| to ensure we keep the mask layer bounds in sync
  // with the native view layer's bounds across size changes.
  native_view->AddObserver(this);
}

void AssistantWebView::DidSuppressNavigation(const GURL& url,
                                             WindowOpenDisposition disposition,
                                             bool from_user_gesture) {
  if (!from_user_gesture)
    return;

  // Deep links are always handled by AssistantController. If the |disposition|
  // indicates a desire to open a new foreground tab, we also defer to the
  // AssistantController so that it can open the |url| in the browser.
  if (assistant::util::IsDeepLinkUrl(url) ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    assistant_controller_->OpenUrl(url);
    return;
  }

  // Otherwise we'll allow our web contents to navigate freely.
  contents_->Navigate(url);
}

void AssistantWebView::RemoveContents() {
  if (!contents_)
    return;

  gfx::NativeView native_view = contents_->GetView()->native_view();
  if (native_view) {
    native_view->RemoveObserver(this);
    native_view->layer()->SetMaskLayer(nullptr);
  }

  views::View* view = contents_->GetView()->view();
  if (view)
    RemoveChildView(view);

  contents_->RemoveObserver(this);
  contents_.reset();
}

}  // namespace ash
