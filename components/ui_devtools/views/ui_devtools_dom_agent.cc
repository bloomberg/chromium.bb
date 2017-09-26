// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/ui_devtools_dom_agent.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/views/ui_devtools_overlay_agent.h"
#include "components/ui_devtools/views/ui_element.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/render_text.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ui_devtools {
namespace {

using namespace ui_devtools::protocol;
// TODO(mhashmi): Make ids reusable

std::unique_ptr<DOM::Node> BuildNode(
    const std::string& name,
    std::unique_ptr<Array<std::string>> attributes,
    std::unique_ptr<Array<DOM::Node>> children,
    int node_ids) {
  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<DOM::Node> node = DOM::Node::create()
                                        .setNodeId(node_ids)
                                        .setBackendNodeId(node_ids)
                                        .setNodeName(name)
                                        .setNodeType(kDomElementNodeType)
                                        .setAttributes(std::move(attributes))
                                        .build();
  node->setChildNodeCount(static_cast<int>(children->length()));
  node->setChildren(std::move(children));
  return node;
}

// TODO(thanhph): Move this function to UIElement::GetAttributes().
std::unique_ptr<Array<std::string>> GetAttributes(UIElement* ui_element) {
  std::unique_ptr<Array<std::string>> attributes = Array<std::string>::create();
  attributes->addItem("name");
  switch (ui_element->type()) {
    case UIElementType::WINDOW: {
      aura::Window* window =
          UIElement::GetBackingElement<aura::Window, WindowElement>(ui_element);
      attributes->addItem(window->GetName());
      attributes->addItem("active");
      attributes->addItem(::wm::IsActiveWindow(window) ? "true" : "false");
      break;
    }
    case UIElementType::WIDGET: {
      views::Widget* widget =
          UIElement::GetBackingElement<views::Widget, WidgetElement>(
              ui_element);
      attributes->addItem(widget->GetName());
      attributes->addItem("active");
      attributes->addItem(widget->IsActive() ? "true" : "false");
      break;
    }
    case UIElementType::VIEW: {
      attributes->addItem(
          UIElement::GetBackingElement<views::View, ViewElement>(ui_element)
              ->GetClassName());
      break;
    }
    default:
      DCHECK(false);
  }
  return attributes;
}

views::Widget* GetWidgetFromWindow(aura::Window* window) {
  return views::Widget::GetWidgetForNativeView(window);
}

std::unique_ptr<DOM::Node> BuildDomNodeFromUIElement(UIElement* root) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (auto* it : root->children())
    children->addItem(BuildDomNodeFromUIElement(it));

  return BuildNode(root->GetTypeName(), GetAttributes(root),
                   std::move(children), root->node_id());
}

// Draw width() x height() of a rectangle if not empty. Otherwise, draw either
// width() or height() if any of them is not empty.
void DrawSizeOfRectangle(const gfx::Rect& hovered_rect,
                         const RectSide drawing_side,
                         gfx::Canvas* canvas,
                         gfx::RenderText* render_text_) {
  base::string16 utf16_text;
  const std::string unit = "dp";

  if (!hovered_rect.IsEmpty()) {
    utf16_text = base::UTF8ToUTF16(hovered_rect.size().ToString() + unit);
  } else if (hovered_rect.height()) {
    // Draw only height() if height() is not empty.
    utf16_text =
        base::UTF8ToUTF16(std::to_string(hovered_rect.height()) + unit);
  } else if (hovered_rect.width()) {
    // Draw only width() if width() is not empty.
    utf16_text = base::UTF8ToUTF16(std::to_string(hovered_rect.width()) + unit);
  } else {
    // If both width() and height() are empty, canvas won't draw size.
    return;
  }
  render_text_->SetText(utf16_text);
  render_text_->SetColor(SK_ColorRED);

  const gfx::Size& text_size = render_text_->GetStringSize();
  gfx::Rect text_rect;
  if (drawing_side == RectSide::LEFT_SIDE) {
    const gfx::Point text_left_side(
        hovered_rect.x() + 1,
        hovered_rect.height() / 2 - text_size.height() / 2 + hovered_rect.y());
    text_rect = gfx::Rect(text_left_side,
                          gfx::Size(text_size.width(), text_size.height()));
  } else if (drawing_side == RectSide::RIGHT_SIDE) {
    const gfx::Point text_right_side(
        hovered_rect.right() - 1 - text_size.width(),
        hovered_rect.height() / 2 - text_size.height() / 2 + hovered_rect.y());
    text_rect = gfx::Rect(text_right_side,
                          gfx::Size(text_size.width(), text_size.height()));
  } else if (drawing_side == RectSide::TOP_SIDE) {
    const gfx::Point text_top_side(
        hovered_rect.x() + hovered_rect.width() / 2 - text_size.width() / 2,
        hovered_rect.y() + 1);
    text_rect = gfx::Rect(text_top_side,
                          gfx::Size(text_size.width(), text_size.height()));
  } else if (drawing_side == RectSide::BOTTOM_SIDE) {
    const gfx::Point text_top_side(
        hovered_rect.x() + hovered_rect.width() / 2 - text_size.width() / 2,
        hovered_rect.bottom() - 1 - text_size.height());
    text_rect = gfx::Rect(text_top_side,
                          gfx::Size(text_size.width(), text_size.height()));
  }
  canvas->FillRect(text_rect, SK_ColorWHITE, SkBlendMode::kColor);
  render_text_->SetDisplayRect(text_rect);
  render_text_->Draw(canvas);
}

void DrawRectGuideLinesOnCanvas(const gfx::Rect& screen_bounds,
                                const gfx::RectF& rect_f,
                                cc::PaintFlags flags,
                                gfx::Canvas* canvas) {
  // Top horizontal dotted line from left to right.
  canvas->DrawLine(gfx::PointF(0.0f, rect_f.y()),
                   gfx::PointF(screen_bounds.right(), rect_f.y()), flags);

  // Bottom horizontal dotted line from left to right.
  canvas->DrawLine(gfx::PointF(0.0f, rect_f.bottom()),
                   gfx::PointF(screen_bounds.right(), rect_f.bottom()), flags);

  // Left vertical dotted line from top to bottom.
  canvas->DrawLine(gfx::PointF(rect_f.x(), 0.0f),
                   gfx::PointF(rect_f.x(), screen_bounds.bottom()), flags);

  // Right vertical dotted line from top to bottom.
  canvas->DrawLine(gfx::PointF(rect_f.right(), 0.0f),
                   gfx::PointF(rect_f.right(), screen_bounds.bottom()), flags);
}

void DrawTextWithAnyBounds(float x1,
                           float y1,
                           float x2,
                           float y2,
                           RectSide side,
                           gfx::Canvas* canvas,
                           gfx::RenderText* render_text) {
  if (x2 > x1 || y2 > y1) {
    DrawSizeOfRectangle(gfx::Rect(x1, y1, x2 - x1, y2 - y1), side, canvas,
                        render_text);
  } else {
    DrawSizeOfRectangle(gfx::Rect(x2, y2, x1 - x2, y1 - y2), side, canvas,
                        render_text);
  }
}

void DrawR1ContainsR2(const gfx::RectF& pinned_rect_f,
                      const gfx::RectF& hovered_rect_f,
                      const cc::PaintFlags& flags,
                      gfx::Canvas* canvas,
                      gfx::RenderText* render_text) {
  // Horizontal left distance line.
  float x1 = pinned_rect_f.x();
  float y1 = pinned_rect_f.y() + pinned_rect_f.height() / 2;
  float x2 = hovered_rect_f.x();
  float y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  // Horizontal right distance line.
  x1 = hovered_rect_f.right();
  y1 = pinned_rect_f.y() + pinned_rect_f.height() / 2;
  x2 = pinned_rect_f.right();
  y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  // Vertical top distance line.
  x1 = pinned_rect_f.x() + pinned_rect_f.width() / 2;
  y1 = pinned_rect_f.y();
  x2 = x1;
  y2 = hovered_rect_f.y();
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);

  // Vertical bottom distance line.
  x1 = pinned_rect_f.x() + pinned_rect_f.width() / 2;
  y1 = hovered_rect_f.bottom();
  x2 = x1;
  y2 = pinned_rect_f.bottom();
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

void DrawR1HorizontalFullLeftR2(const gfx::RectF& pinned_rect_f,
                                const gfx::RectF& hovered_rect_f,
                                const cc::PaintFlags& flags,
                                gfx::Canvas* canvas,
                                gfx::RenderText* render_text) {
  // Horizontal left distance line.
  float x1 = hovered_rect_f.right();
  float y1 = hovered_rect_f.y() + hovered_rect_f.height() / 2;
  float x2 = pinned_rect_f.x();
  float y2 = y1;
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);
}

void DrawR1TopFullLeftR2(const gfx::RectF& pinned_rect_f,
                         const gfx::RectF& hovered_rect_f,
                         const cc::PaintFlags& flags,
                         gfx::Canvas* canvas_,
                         gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.x() + hovered_rect_f.width();
  float y1 = hovered_rect_f.y() + hovered_rect_f.height() / 2;
  float x2 = pinned_rect_f.x();
  float y2 = hovered_rect_f.y() + hovered_rect_f.height() / 2;

  // Horizontal left dotted line.
  canvas_->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas_,
                        render_text);
  x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  y1 = hovered_rect_f.y() + hovered_rect_f.height();
  x2 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  y2 = pinned_rect_f.y();

  // Vertical left dotted line.
  canvas_->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas_,
                        render_text);
}

void DrawR1BottomFullLeftR2(const gfx::RectF& pinned_rect_f,
                            const gfx::RectF& hovered_rect_f,
                            const cc::PaintFlags& flags,
                            gfx::Canvas* canvas,
                            gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.right();
  float y1 = hovered_rect_f.y() + hovered_rect_f.height() / 2;
  float x2 = pinned_rect_f.x();
  float y2 = y1;

  // Horizontal left distance line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::BOTTOM_SIDE, canvas,
                        render_text);

  x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  y1 = pinned_rect_f.bottom();
  x2 = x1;
  y2 = hovered_rect_f.y();

  // Vertical left distance line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

void DrawR1TopPartialLeftR2(const gfx::RectF& pinned_rect_f,
                            const gfx::RectF& hovered_rect_f,
                            const cc::PaintFlags& flags,
                            gfx::Canvas* canvas,
                            gfx::RenderText* render_text) {
  float x1 = hovered_rect_f.x() + hovered_rect_f.width() / 2;
  float y1 = hovered_rect_f.bottom();
  float x2 = x1;
  float y2 = pinned_rect_f.y();

  // Vertical left dotted line.
  canvas->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2), flags);
  DrawTextWithAnyBounds(x1, y1, x2, y2, RectSide::LEFT_SIDE, canvas,
                        render_text);
}

}  // namespace

UIDevToolsDOMAgent::UIDevToolsDOMAgent()
    : is_building_tree_(false),
      show_size_on_canvas_(false),
      highlight_rect_config_(HighlightRectsConfiguration::NO_DRAW) {
  aura::Env::GetInstance()->AddObserver(this);
}

UIDevToolsDOMAgent::~UIDevToolsDOMAgent() {
  aura::Env::GetInstance()->RemoveObserver(this);
  Reset();
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::disable() {
  Reset();
  return ui_devtools::protocol::Response::OK();
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::getDocument(
    std::unique_ptr<ui_devtools::protocol::DOM::Node>* out_root) {
  *out_root = BuildInitialTree();
  return ui_devtools::protocol::Response::OK();
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::hideHighlight() {
  if (layer_for_highlighting_ && layer_for_highlighting_->visible())
    layer_for_highlighting_->SetVisible(false);
  return ui_devtools::protocol::Response::OK();
}

ui_devtools::protocol::Response
UIDevToolsDOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = protocol::Array<int>::create();
  for (size_t index = 0; index < backend_node_ids->length(); ++index)
    (*result)->addItem(backend_node_ids->get(index));
  return ui_devtools::protocol::Response::OK();
}

void UIDevToolsDOMAgent::OnUIElementAdded(UIElement* parent, UIElement* child) {
  // When parent is null, only need to update |node_id_to_ui_element_|.
  if (!parent) {
    node_id_to_ui_element_[child->node_id()] = child;
    return;
  }
  // If tree is being built, don't add child to dom tree again.
  if (is_building_tree_)
    return;
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.end() - 1) ? 0 : (*std::next(iter))->node_id();
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildTreeForUIElement(child));
}

void UIDevToolsDOMAgent::OnUIElementReordered(UIElement* parent,
                                              UIElement* child) {
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  RemoveDomNode(child);
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildDomNodeFromUIElement(child));
}

void UIDevToolsDOMAgent::OnUIElementRemoved(UIElement* ui_element) {
  DCHECK(node_id_to_ui_element_.count(ui_element->node_id()));

  RemoveDomNode(ui_element);
  node_id_to_ui_element_.erase(ui_element->node_id());
}

void UIDevToolsDOMAgent::OnUIElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void UIDevToolsDOMAgent::AddObserver(UIDevToolsDOMAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void UIDevToolsDOMAgent::RemoveObserver(UIDevToolsDOMAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIElement* UIDevToolsDOMAgent::GetElementFromNodeId(int node_id) {
  return node_id_to_ui_element_[node_id];
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::HighlightNode(
    int node_id,
    bool show_size) {
  if (!layer_for_highlighting_) {
    layer_for_highlighting_.reset(new ui::Layer(ui::LayerType::LAYER_TEXTURED));
    layer_for_highlighting_->set_name("HighlightingLayer");
    layer_for_highlighting_->set_delegate(this);
    layer_for_highlighting_->SetFillsBoundsOpaquely(false);
  }
  std::pair<aura::Window*, gfx::Rect> window_and_bounds =
      node_id_to_ui_element_.count(node_id)
          ? node_id_to_ui_element_[node_id]->GetNodeWindowAndBounds()
          : std::make_pair<aura::Window*, gfx::Rect>(nullptr, gfx::Rect());

  if (!window_and_bounds.first)
    return ui_devtools::protocol::Response::Error("No node found with that id");

  highlight_rect_config_ = HighlightRectsConfiguration::NO_DRAW;
  show_size_on_canvas_ = show_size;
  UpdateHighlight(window_and_bounds);

  if (!layer_for_highlighting_->visible())
    layer_for_highlighting_->SetVisible(true);

  return ui_devtools::protocol::Response::OK();
}

int UIDevToolsDOMAgent::FindElementIdTargetedByPoint(
    const gfx::Point& p,
    aura::Window* root_window) const {
  aura::Window* targeted_window = root_window->GetEventHandlerForPoint(p);
  if (!targeted_window)
    return 0;

  views::Widget* targeted_widget =
      views::Widget::GetWidgetForNativeWindow(targeted_window);
  if (!targeted_widget) {
    return window_element_root_->FindUIElementIdForBackendElement<aura::Window>(
        targeted_window);
  }

  views::View* root_view = targeted_widget->GetRootView();
  DCHECK(root_view);

  gfx::Point point_in_targeted_window(p);
  aura::Window::ConvertPointToTarget(root_window, targeted_window,
                                     &point_in_targeted_window);
  views::View* targeted_view =
      root_view->GetEventHandlerForPoint(point_in_targeted_window);
  DCHECK(targeted_view);
  return window_element_root_->FindUIElementIdForBackendElement<views::View>(
      targeted_view);
}

void UIDevToolsDOMAgent::ShowDistancesInHighlightOverlay(int pinned_id,
                                                         int element_id) {
  const std::pair<aura::Window*, gfx::Rect> pair_r2(
      node_id_to_ui_element_[element_id]->GetNodeWindowAndBounds());
  const std::pair<aura::Window*, gfx::Rect> pair_r1(
      node_id_to_ui_element_[pinned_id]->GetNodeWindowAndBounds());
  gfx::Rect r2(pair_r2.second);
  gfx::Rect r1(pair_r1.second);
  pinned_rect_ = r1;

  is_swap_ = false;
  if (r1.x() > r2.x()) {
    is_swap_ = true;
    std::swap(r1, r2);
  }
  if (r1.Contains(r2)) {
    highlight_rect_config_ = HighlightRectsConfiguration::R1_CONTAINS_R2;
  } else if (r1.right() <= r2.x()) {
    if ((r1.y() <= r2.y() && r2.y() <= r1.bottom()) ||
        (r1.y() <= r2.bottom() && r2.bottom() <= r1.bottom()) ||
        (r2.y() <= r1.y() && r1.y() <= r2.bottom()) ||
        (r2.y() <= r1.bottom() && r1.bottom() <= r2.bottom())) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_HORIZONTAL_FULL_LEFT_R2;
    } else if (r1.bottom() <= r2.y()) {
      highlight_rect_config_ = HighlightRectsConfiguration::R1_TOP_FULL_LEFT_R2;
    } else if (r1.y() >= r2.bottom()) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_BOTTOM_FULL_LEFT_R2;
    }
  } else if (r1.x() <= r2.x() && r2.x() <= r1.right()) {
    if (r1.bottom() <= r2.y()) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_TOP_PARTIAL_LEFT_R2;
    } else if (r1.y() >= r2.bottom()) {
      highlight_rect_config_ =
          HighlightRectsConfiguration::R1_BOTTOM_PARTIAL_LEFT_R2;
    } else if (r1.Intersects(r2)) {
      highlight_rect_config_ = HighlightRectsConfiguration::R1_INTERSECTS_R2;
    } else {
      NOTREACHED();
    }
  } else {
    highlight_rect_config_ = HighlightRectsConfiguration::NO_DRAW;
  }
}

int UIDevToolsDOMAgent::GetParentIdOfNodeId(int node_id) const {
  DCHECK(node_id_to_ui_element_.count(node_id));
  const UIElement* element = node_id_to_ui_element_.at(node_id);
  if (element->parent() && element->parent() != window_element_root_.get())
    return element->parent()->node_id();
  return 0;
}

void UIDevToolsDOMAgent::OnPaintLayer(const ui::PaintContext& context) {
  const gfx::Rect& screen_bounds(layer_for_highlighting_->bounds());
  ui::PaintRecorder recorder(context, screen_bounds.size());
  gfx::Canvas* canvas = recorder.canvas();
  gfx::RectF hovered_rect_f(hovered_rect_);

  cc::PaintFlags flags;
  flags.setColor(SK_ColorBLUE);
  flags.setStrokeWidth(1.0f);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  constexpr SkScalar intervals[] = {1.f, 4.f};
  flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

  if (!render_text_) {
    render_text_ =
        base::WrapUnique<gfx::RenderText>(gfx::RenderText::CreateInstance());
  }
  // Display guide lines if |highlight_rect_config_| is NO_DRAW.
  if (highlight_rect_config_ == HighlightRectsConfiguration::NO_DRAW) {
    hovered_rect_f.Inset(gfx::InsetsF(-1));
    DrawRectGuideLinesOnCanvas(screen_bounds, hovered_rect_f, flags, canvas);
    // Draw |hovered_rect_f| bounds.
    flags.setPathEffect(0);
    canvas->DrawRect(hovered_rect_f, flags);

    // Display size of the rectangle after mouse click.
    if (show_size_on_canvas_) {
      DrawSizeOfRectangle(hovered_rect_, RectSide::BOTTOM_SIDE, canvas,
                          render_text_.get());
    }
    return;
  }
  flags.setPathEffect(0);
  flags.setColor(SK_ColorBLUE);

  gfx::RectF pinned_rect_f(pinned_rect_);

  // Draw |pinned_rect_f| bounds in blue.
  canvas->DrawRect(pinned_rect_f, flags);

  // Draw |hovered_rect_f| bounds in green.
  flags.setColor(SK_ColorGREEN);
  canvas->DrawRect(hovered_rect_f, flags);

  // Draw distances in red colour.
  flags.setPathEffect(0);
  flags.setColor(SK_ColorRED);

  // Make sure |pinned_rect_f| stays on the right or below of |hovered_rect_f|.
  if (pinned_rect_.x() < hovered_rect_.x() ||
      (pinned_rect_.x() == hovered_rect_.x() &&
       pinned_rect_.y() < hovered_rect_.y())) {
    std::swap(pinned_rect_f, hovered_rect_f);
  }

  switch (highlight_rect_config_) {
    case HighlightRectsConfiguration::R1_CONTAINS_R2:
      DrawR1ContainsR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                       render_text_.get());
      return;
    case HighlightRectsConfiguration::R1_HORIZONTAL_FULL_LEFT_R2:
      DrawR1HorizontalFullLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                                 render_text_.get());
      return;
    case HighlightRectsConfiguration::R1_TOP_FULL_LEFT_R2:
      DrawR1TopFullLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                          render_text_.get());

      // Draw 4 guide lines along distance lines.
      flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

      // Bottom horizontal dotted line from left to right.
      canvas->DrawLine(
          gfx::PointF(0.0f, hovered_rect_f.bottom()),
          gfx::PointF(screen_bounds.right(), hovered_rect_f.bottom()), flags);

      // Right vertical dotted line from top to bottom.
      canvas->DrawLine(
          gfx::PointF(hovered_rect_f.right(), 0.0f),
          gfx::PointF(hovered_rect_f.right(), screen_bounds.bottom()), flags);

      // Top horizontal dotted line from left to right.
      canvas->DrawLine(gfx::PointF(0.0f, pinned_rect_f.y()),
                       gfx::PointF(screen_bounds.right(), pinned_rect_f.y()),
                       flags);

      // Left vertical dotted line from top to bottom.
      canvas->DrawLine(gfx::PointF(pinned_rect_f.x(), 0.0f),
                       gfx::PointF(pinned_rect_f.x(), screen_bounds.bottom()),
                       flags);
      return;
    case HighlightRectsConfiguration::R1_BOTTOM_FULL_LEFT_R2:
      DrawR1BottomFullLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                             render_text_.get());

      // Draw 2 guide lines along distance lines.
      flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

      // Top horizontal dotted line from left to right.
      canvas->DrawLine(
          gfx::PointF(0.0f, pinned_rect_f.bottom()),
          gfx::PointF(screen_bounds.right(), pinned_rect_f.bottom()), flags);

      // Left vertical dotted line from top to bottom.
      canvas->DrawLine(gfx::PointF(pinned_rect_f.x(), 0.0f),
                       gfx::PointF(pinned_rect_f.x(), screen_bounds.bottom()),
                       flags);
      return;
    case HighlightRectsConfiguration::R1_TOP_PARTIAL_LEFT_R2:
      DrawR1TopPartialLeftR2(pinned_rect_f, hovered_rect_f, flags, canvas,
                             render_text_.get());

      // Draw 1 guide line along distance lines.
      flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

      // Top horizontal dotted line from left to right.
      canvas->DrawLine(gfx::PointF(0.0f, pinned_rect_f.y()),
                       gfx::PointF(screen_bounds.right(), pinned_rect_f.y()),
                       flags);
      return;
    case HighlightRectsConfiguration::R1_BOTTOM_PARTIAL_LEFT_R2:
      NOTIMPLEMENTED();
      return;
    case HighlightRectsConfiguration::R1_INTERSECTS_R2:
      NOTIMPLEMENTED();
      return;
    default:
      NOTREACHED();
      return;
  }
}

void UIDevToolsDOMAgent::OnHostInitialized(aura::WindowTreeHost* host) {
  root_windows_.push_back(host->window());
}

void UIDevToolsDOMAgent::OnElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

std::unique_ptr<ui_devtools::protocol::DOM::Node>
UIDevToolsDOMAgent::BuildInitialTree() {
  is_building_tree_ = true;
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  // TODO(thanhph): Root of UIElement tree shoudn't be WindowElement
  // but maybe a new different element type.
  window_element_root_ =
      base::MakeUnique<WindowElement>(nullptr, this, nullptr);

  for (aura::Window* window : root_windows()) {
    UIElement* window_element =
        new WindowElement(window, this, window_element_root_.get());

    children->addItem(BuildTreeForUIElement(window_element));
    window_element_root_->AddChild(window_element);
  }
  std::unique_ptr<ui_devtools::protocol::DOM::Node> root_node = BuildNode(
      "root", nullptr, std::move(children), window_element_root_->node_id());
  is_building_tree_ = false;
  return root_node;
}

std::unique_ptr<ui_devtools::protocol::DOM::Node>
UIDevToolsDOMAgent::BuildTreeForUIElement(UIElement* ui_element) {
  if (ui_element->type() == UIElementType::WINDOW) {
    return BuildTreeForWindow(
        ui_element,
        UIElement::GetBackingElement<aura::Window, WindowElement>(ui_element));
  } else if (ui_element->type() == UIElementType::WIDGET) {
    return BuildTreeForRootWidget(
        ui_element,
        UIElement::GetBackingElement<views::Widget, WidgetElement>(ui_element));
  } else if (ui_element->type() == UIElementType::VIEW) {
    return BuildTreeForView(
        ui_element,
        UIElement::GetBackingElement<views::View, ViewElement>(ui_element));
  }
  return nullptr;
}

std::unique_ptr<DOM::Node> UIDevToolsDOMAgent::BuildTreeForWindow(
    UIElement* window_element_root,
    aura::Window* window) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  views::Widget* widget = GetWidgetFromWindow(window);
  if (widget) {
    UIElement* widget_element =
        new WidgetElement(widget, this, window_element_root);

    children->addItem(BuildTreeForRootWidget(widget_element, widget));
    window_element_root->AddChild(widget_element);
  }
  for (aura::Window* child : window->children()) {
    UIElement* window_element =
        new WindowElement(child, this, window_element_root);

    children->addItem(BuildTreeForWindow(window_element, child));
    window_element_root->AddChild(window_element);
  }
  std::unique_ptr<ui_devtools::protocol::DOM::Node> node =
      BuildNode("Window", GetAttributes(window_element_root),
                std::move(children), window_element_root->node_id());
  return node;
}

std::unique_ptr<DOM::Node> UIDevToolsDOMAgent::BuildTreeForRootWidget(
    UIElement* widget_element,
    views::Widget* widget) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  UIElement* view_element =
      new ViewElement(widget->GetRootView(), this, widget_element);

  children->addItem(BuildTreeForView(view_element, widget->GetRootView()));
  widget_element->AddChild(view_element);

  std::unique_ptr<ui_devtools::protocol::DOM::Node> node =
      BuildNode("Widget", GetAttributes(widget_element), std::move(children),
                widget_element->node_id());
  return node;
}

std::unique_ptr<DOM::Node> UIDevToolsDOMAgent::BuildTreeForView(
    UIElement* view_element,
    views::View* view) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  for (auto* child : view->GetChildrenInZOrder()) {
    UIElement* view_element_child = new ViewElement(child, this, view_element);

    children->addItem(BuildTreeForView(view_element_child, child));
    view_element->AddChild(view_element_child);
  }
  std::unique_ptr<ui_devtools::protocol::DOM::Node> node =
      BuildNode("View", GetAttributes(view_element), std::move(children),
                view_element->node_id());
  return node;
}

void UIDevToolsDOMAgent::RemoveDomNode(UIElement* ui_element) {
  for (auto* child_element : ui_element->children())
    RemoveDomNode(child_element);
  frontend()->childNodeRemoved(ui_element->parent()->node_id(),
                               ui_element->node_id());
}

void UIDevToolsDOMAgent::Reset() {
  is_building_tree_ = false;
  render_text_.reset();
  layer_for_highlighting_.reset();
  window_element_root_.reset();
  node_id_to_ui_element_.clear();
  observers_.Clear();
}

void UIDevToolsDOMAgent::UpdateHighlight(
    const std::pair<aura::Window*, gfx::Rect>& window_and_bounds) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          window_and_bounds.first);
  aura::Window* root = window_and_bounds.first->GetRootWindow();
  layer_for_highlighting_->SetBounds(root->bounds());
  layer_for_highlighting_->SchedulePaint(root->bounds());

  if (root->layer() != layer_for_highlighting_->parent())
    root->layer()->Add(layer_for_highlighting_.get());
  else
    root->layer()->StackAtTop(layer_for_highlighting_.get());

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root);
  hovered_rect_ = window_and_bounds.second;
  gfx::Point origin = hovered_rect_.origin();
  screen_position_client->ConvertPointFromScreen(root, &origin);
  hovered_rect_.set_origin(origin);
}

}  // namespace ui_devtools
