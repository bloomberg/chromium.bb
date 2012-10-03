// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_frame_simple.h"

#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "grit/ui_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/shared_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/path.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

typedef ConstrainedWindowFrameSimple::HeaderViews HeaderViews;

// A layout manager that lays out the header view with proper padding,
// and sized to the widget's client view.
class HeaderLayout : public views::LayoutManager {
 public:
  explicit HeaderLayout(views::Widget* container) : container_(container) {}
  virtual ~HeaderLayout() {}

  // Overridden from LayoutManager
  virtual void Layout(views::View* host);
  virtual gfx::Size GetPreferredSize(views::View* host);

 private:
  views::Widget* container_;

  DISALLOW_COPY_AND_ASSIGN(HeaderLayout);
};

void HeaderLayout::Layout(views::View* host) {
  if (!host->has_children())
    return;

  int horizontal_padding = ConstrainedWindow::kHorizontalPadding;
  int vertical_padding = ConstrainedWindow::kVerticalPadding;
  int row_padding = ConstrainedWindow::kRowPadding;

  views::View* header = host->child_at(0);
  gfx::Size preferred_size = GetPreferredSize(host);
  int width = preferred_size.width() - 2 * horizontal_padding;
  int height = preferred_size.height() - vertical_padding - row_padding;

  header->SetBounds(horizontal_padding, vertical_padding, width, height);
}

gfx::Size HeaderLayout::GetPreferredSize(views::View* host) {
  int horizontal_padding = ConstrainedWindow::kHorizontalPadding;
  int vertical_padding = ConstrainedWindow::kVerticalPadding;
  int row_padding = ConstrainedWindow::kRowPadding;

  views::View* header = host->child_at(0);
  views::View* client_view = container_->client_view();

  gfx::Size header_size = header ? header->GetPreferredSize() : gfx::Size();
  gfx::Size client_size =
      client_view ? client_view->GetPreferredSize() : gfx::Size();
  int width = std::max(client_size.width(), header_size.width()) +
      2 * horizontal_padding;
  int height = vertical_padding + header_size.height() + row_padding;
  return gfx::Size(width, height);
}

}  // namespace

ConstrainedWindowFrameSimple::HeaderViews::HeaderViews(
    views::View* header,
    views::Label* title_label,
    views::Button* close_button)
    : header(header),
      title_label(title_label),
      close_button(close_button) {
  DCHECK(header);
}

ConstrainedWindowFrameSimple::ConstrainedWindowFrameSimple(
    ConstrainedWindowViews* container)
    : container_(container) {
  container_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);

  layout_ = new HeaderLayout(container_);
  SetLayoutManager(layout_);

  SetHeaderView(CreateDefaultHeaderView());

  set_background(views::Background::CreateSolidBackground(
      ConstrainedWindow::GetBackgroundColor()));
}

ConstrainedWindowFrameSimple::~ConstrainedWindowFrameSimple() {
}

void ConstrainedWindowFrameSimple::SetHeaderView(HeaderViews* header_views)
{
  RemoveAllChildViews(true);

  header_views_.reset(header_views);

  AddChildView(header_views_->header);
}

HeaderViews* ConstrainedWindowFrameSimple::CreateDefaultHeaderView() {
  views::View* header_view = new views::View;

  views::GridLayout* grid_layout = new views::GridLayout(header_view);
  header_view->SetLayoutManager(grid_layout);

  views::ColumnSet* header_cs = grid_layout->AddColumnSet(0);
  header_cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                       views::GridLayout::USE_PREF, 0, 0);  // Title.
  header_cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  header_cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                       views::GridLayout::USE_PREF, 0, 0);  // Close Button.

  // Header row.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  grid_layout->StartRow(0, 0);
  views::Label* title_label = new views::Label();
  title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label->SetFont(rb.GetFont(ConstrainedWindow::kTitleFontStyle));
  title_label->SetEnabledColor(ConstrainedWindow::GetTextColor());
  title_label->SetText(container_->widget_delegate()->GetWindowTitle());
  grid_layout->AddView(title_label);

  views::Button* close_button = CreateCloseButton();
  grid_layout->AddView(close_button);

  return new HeaderViews(header_view, title_label, close_button);
}

gfx::Rect ConstrainedWindowFrameSimple::GetBoundsForClientView() const {
  int horizontal_padding = ConstrainedWindow::kHorizontalPadding;
  int vertical_padding = ConstrainedWindow::kVerticalPadding;
  int row_padding = ConstrainedWindow::kRowPadding;
  gfx::Size header_size =
      header_views_->header ?
      header_views_->header->GetPreferredSize() : gfx::Size();

  return gfx::Rect(horizontal_padding,
                   vertical_padding + header_size.height() + row_padding,
                   std::max(0, width() - 2 * horizontal_padding),
                   std::max(0, (height() - 2 * vertical_padding -
                                header_size.height() - row_padding)));
}

gfx::Rect ConstrainedWindowFrameSimple::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int horizontal_padding = ConstrainedWindow::kHorizontalPadding;
  int vertical_padding = ConstrainedWindow::kVerticalPadding;
  int row_padding = ConstrainedWindow::kRowPadding;
  gfx::Size header_size =
      header_views_->header ?
      header_views_->header->GetPreferredSize() : gfx::Size();

  int x = client_bounds.x() - horizontal_padding;
  int y = client_bounds.y() - vertical_padding - header_size.height() -
      row_padding;
  int width = client_bounds.width() + 2 * horizontal_padding;
  int height = client_bounds.height() + 2 * vertical_padding +
      header_size.height() + row_padding;

  return gfx::Rect(x, y, width, height);
}

int ConstrainedWindowFrameSimple::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  return HTCLIENT;
}

void ConstrainedWindowFrameSimple::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
  SkRect rect = {0, 0, size.width() - 1, size.height() - 1};
  SkScalar radius = SkIntToScalar(ConstrainedWindow::kBorderRadius);
  SkScalar radii[8] = {radius, radius, radius, radius,
                       radius, radius, radius, radius};

  // NB: We're not using the addRoundRect uniform radius overload as it
  // mishandles the bottom corners on Windows
  window_mask->addRoundRect(rect, radii);
}

void ConstrainedWindowFrameSimple::ResetWindowControls() {
}

void ConstrainedWindowFrameSimple::UpdateWindowIcon() {
}

void ConstrainedWindowFrameSimple::UpdateWindowTitle() {
  if (!header_views_->title_label)
    return;

  string16 text = container_->widget_delegate()->GetWindowTitle();
  header_views_->title_label->SetText(text);
}

gfx::Size ConstrainedWindowFrameSimple::GetPreferredSize() {
  return container_->non_client_view()->GetWindowBoundsForClientBounds(
      gfx::Rect(container_->client_view()->GetPreferredSize())).size();
}

void ConstrainedWindowFrameSimple::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (header_views_->close_button && sender == header_views_->close_button)
    sender->GetWidget()->Close();
}

views::ImageButton* ConstrainedWindowFrameSimple::CreateCloseButton() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X));
  close_button->SetImage(views::CustomButton::BS_HOT,
                          rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X_HOVER));
  close_button->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X_HOVER));
  return close_button;
}
