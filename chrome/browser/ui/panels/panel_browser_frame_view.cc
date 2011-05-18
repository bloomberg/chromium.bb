// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_frame_view.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"
#include "views/painter.h"
#include "views/window/window.h"
#include "views/window/window_shape.h"

#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif

namespace {

// The height in pixels of the title bar.
const int kTitleBarHeight = 24;

// The thickness in pixels of the frame border.
const int kFrameBorderThickness = 1;

// The spacing in pixels between the icon and the border/text.
const int kIconSpacing = 4;

// The height and width in pixels of the icon.
const int kIconSize = 16;

// The spacing in pixels between buttons or the button and the adjacent control.
const int kButtonSpacing = 6;

struct ButtonResources {
  SkBitmap* normal_image;
  SkBitmap* mask_image;
  SkBitmap* hover_image;
  SkBitmap* pushed_image;

  void SetResources(int normal_image_id, int mask_image_id, int hover_image_id,
                    int pushed_image_id) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    normal_image = rb.GetBitmapNamed(normal_image_id);
    mask_image = mask_image_id ? rb.GetBitmapNamed(mask_image_id) : NULL;
    hover_image = rb.GetBitmapNamed(hover_image_id);
    pushed_image = rb.GetBitmapNamed(pushed_image_id);
  }
};

struct EdgeResources {
  SkBitmap* top_left;
  SkBitmap* top;
  SkBitmap* top_right;
  SkBitmap* right;
  SkBitmap* bottom_right;
  SkBitmap* bottom;
  SkBitmap* bottom_left;
  SkBitmap* left;

  void SetResources(int top_left_id, int top_id, int top_right_id, int right_id,
                    int bottom_right_id, int bottom_id, int bottom_left_id,
                    int left_id) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    top_left = rb.GetBitmapNamed(top_left_id);
    top = rb.GetBitmapNamed(top_id);
    top_right = rb.GetBitmapNamed(top_right_id);
    right = rb.GetBitmapNamed(right_id);
    bottom_right = rb.GetBitmapNamed(bottom_right_id);
    bottom = rb.GetBitmapNamed(bottom_id);
    bottom_left = rb.GetBitmapNamed(bottom_left_id);
    left = rb.GetBitmapNamed(left_id);
  }
};

ButtonResources options_button_resources;
ButtonResources close_button_resources;
EdgeResources frame_edges;
EdgeResources client_edges;
gfx::Font* active_font = NULL;
gfx::Font* inactive_font = NULL;

void LoadImageResources() {
  options_button_resources.SetResources(
      IDR_BALLOON_WRENCH, 0, IDR_BALLOON_WRENCH_H, IDR_BALLOON_WRENCH_P);

  close_button_resources.SetResources(
      IDR_TAB_CLOSE, IDR_TAB_CLOSE_MASK, IDR_TAB_CLOSE_H, IDR_TAB_CLOSE_P);

  frame_edges.SetResources(
      IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_CENTER,
      IDR_WINDOW_TOP_RIGHT_CORNER, IDR_WINDOW_RIGHT_SIDE,
      IDR_WINDOW_BOTTOM_RIGHT_CORNER, IDR_WINDOW_BOTTOM_CENTER,
      IDR_WINDOW_BOTTOM_LEFT_CORNER, IDR_WINDOW_LEFT_SIDE);

  client_edges.SetResources(
      IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER,
      IDR_APP_TOP_RIGHT, IDR_CONTENT_RIGHT_SIDE,
      IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
      IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE);
}

void EnsureResourcesInitialized() {
  static bool resources_initialized = false;
  if (resources_initialized)
    return;
  resources_initialized = true;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  active_font = new gfx::Font(rb.GetFont(ResourceBundle::BoldFont));
  inactive_font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));

  LoadImageResources();
}

} // namespace

PanelBrowserFrameView::PanelBrowserFrameView(BrowserFrame* frame,
                                             PanelBrowserView* browser_view)
    : BrowserNonClientFrameView(),
      frame_(frame),
      browser_view_(browser_view),
      paint_state_(NOT_PAINTED),
      options_button_(NULL),
      close_button_(NULL),
      title_icon_(NULL),
      title_label_(NULL) {
  EnsureResourcesInitialized();
  frame_->set_frame_type(views::Window::FRAME_TYPE_FORCE_CUSTOM);

  options_button_ = new views::MenuButton(NULL, std::wstring(), this, false);
  options_button_->SetIcon(*(options_button_resources.normal_image));
  options_button_->SetHoverIcon(*(options_button_resources.hover_image));
  options_button_->SetPushedIcon(*(options_button_resources.pushed_image));
  options_button_->set_alignment(views::TextButton::ALIGN_CENTER);
  options_button_->set_border(NULL);
  // TODO(jianli): Hide options button by default and show it when mouse is over
  // panel.
  // options_button_->SetVisible(false);
  AddChildView(options_button_);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          close_button_resources.normal_image);
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          close_button_resources.hover_image);
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          close_button_resources.pushed_image);
  close_button_->SetTooltipText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_TOOLTIP_CLOSE_TAB)));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  title_icon_ = new TabIconView(this);
  title_icon_->set_is_light(true);
  AddChildView(title_icon_);

  title_label_ = new views::Label(std::wstring());
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(title_label_);
}

PanelBrowserFrameView::~PanelBrowserFrameView() {
}

gfx::Rect PanelBrowserFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  // Panels never show a tab strip.
  NOTREACHED();
  return gfx::Rect();
}

int PanelBrowserFrameView::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  // This is not needed since we do not show tab strip for the panel.
  return 0;
}

void PanelBrowserFrameView::UpdateThrobber(bool running) {
  // Tells the title icon to update the throbber when we need to show the
  // animation to indicate we're still loading.
  title_icon_->Update();
}

gfx::Rect PanelBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect PanelBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int PanelBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame_->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  if (close_button_->IsVisible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point,
      NonClientBorderThickness(), NonClientBorderThickness(),
      0, 0,
      frame_->window_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void PanelBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                          gfx::Path* window_mask) {
  views::GetDefaultWindowMask(size, window_mask);
}

void PanelBrowserFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void PanelBrowserFrameView::ResetWindowControls() {
  // The close button isn't affected by this constraint.
}

void PanelBrowserFrameView::UpdateWindowIcon() {
  title_icon_->SchedulePaint();
}

void PanelBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  // The font and color need to be updated when the panel becomes active or
  // inactive.
  UpdateControlStyles(browser_view_->panel()->IsActive() ?
                      PAINT_AS_ACTIVE : PAINT_AS_INACTIVE);
  PaintFrameBorder(canvas);
  PaintClientEdge(canvas);
}

void PanelBrowserFrameView::OnThemeChanged() {
  LoadImageResources();
}

void PanelBrowserFrameView::Layout() {
  // Layout the close button.
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - kFrameBorderThickness - kButtonSpacing -
          close_button_size.width(),
      (NonClientTopBorderHeight() - close_button_size.height()) / 2,
      close_button_size.width(),
      close_button_size.height());

  // Layout the options button.
  gfx::Size options_button_size = options_button_->GetPreferredSize();
  options_button_->SetBounds(
      close_button_->x() - kButtonSpacing - options_button_size.width(),
      (NonClientTopBorderHeight() - options_button_size.height()) / 2,
      options_button_size.width(),
      options_button_size.height());

  // Layout the icon.
  int icon_y = (NonClientTopBorderHeight() - kIconSize) / 2;
  title_icon_->SetBounds(
      kFrameBorderThickness + kIconSpacing,
      icon_y,
      kIconSize,
      kIconSize);

  // Layout the title.
  int title_x = title_icon_->bounds().right() + kIconSpacing;
  int title_height = BrowserFrame::GetTitleFont().GetHeight();
  title_label_->SetBounds(
      title_x,
      icon_y + ((kIconSize - title_height - 1) / 2),
      std::max(0, options_button_->x() - kButtonSpacing - title_x),
      title_height);

  // Calculate the client area bounds.
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  client_view_bounds_.SetRect(
      border_thickness,
      top_height,
      std::max(0, width() - (2 * border_thickness)),
      std::max(0, height() - top_height - border_thickness));
}

void PanelBrowserFrameView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TITLEBAR;
}

bool PanelBrowserFrameView::OnMousePressed(const views::MouseEvent& event) {
  if (browser_view_->OnTitleBarMousePressed(event))
    return true;
  return BrowserNonClientFrameView::OnMousePressed(event);
}

bool PanelBrowserFrameView::OnMouseDragged(const views::MouseEvent& event) {
  if (browser_view_->OnTitleBarMouseDragged(event))
    return true;
  return BrowserNonClientFrameView::OnMouseDragged(event);
}

void PanelBrowserFrameView::OnMouseReleased(const views::MouseEvent& event) {
  if (browser_view_->OnTitleBarMouseReleased(event))
    return;
  BrowserNonClientFrameView::OnMouseReleased(event);
}

void PanelBrowserFrameView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender == close_button_)
    frame_->Close();
}

void PanelBrowserFrameView::RunMenu(View* source, const gfx::Point& pt) {
  CreateOrUpdateOptionsMenu();
  options_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

bool PanelBrowserFrameView::IsCommandIdChecked(int command_id) const {
  // Nothing in the menu is checked.
  return false;
}

bool PanelBrowserFrameView::IsCommandIdEnabled(int command_id) const {
  // All the menu options are always enabled.
  return true;
}

bool PanelBrowserFrameView::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

void PanelBrowserFrameView::ExecuteCommand(int command_id) {
  switch (command_id) {
    case COMMAND_MINIMIZE_ALL:
      browser_view_->panel()->manager()->MinimizeAll();
      break;
    case COMMAND_RESTORE_ALL:
      browser_view_->panel()->manager()->RestoreAll();
      break;
    case COMMAND_CLOSE_ALL:
      browser_view_->panel()->manager()->RemoveAllActive();
      break;
    case COMMAND_ABOUT:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool PanelBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view_->GetSelectedTabContents();
  return current_tab ? current_tab->is_loading() : false;
}

SkBitmap PanelBrowserFrameView::GetFaviconForTabIconView() {
  return frame_->window_delegate()->GetWindowIcon();
}

int PanelBrowserFrameView::NonClientBorderThickness() const {
  return kFrameBorderThickness + kClientEdgeThickness;
}

int PanelBrowserFrameView::NonClientTopBorderHeight() const {
  return kFrameBorderThickness + kTitleBarHeight + kClientEdgeThickness;
}

void PanelBrowserFrameView::UpdateControlStyles(PaintState paint_state) {
  DCHECK(paint_state != NOT_PAINTED);

  if (paint_state == paint_state_)
    return;
  paint_state_ = paint_state;

  SkColor title_color = GetThemeProvider()->
      GetColor(paint_state == PAINT_AS_ACTIVE ?
          ThemeService::COLOR_TAB_TEXT :
          ThemeService::COLOR_BACKGROUND_TAB_TEXT);
  title_label_->SetColor(title_color);
  title_label_->SetFont(
      paint_state == PAINT_AS_ACTIVE ? *active_font : *inactive_font);
  close_button_->SetBackground(title_color,
      close_button_resources.normal_image,
      close_button_resources.mask_image);
}

void PanelBrowserFrameView::PaintFrameBorder(gfx::Canvas* canvas) {
  ThemeProvider* theme_provider  = GetThemeProvider();
  SkBitmap* theme_frame = theme_provider->GetBitmapNamed(
      (paint_state_ == PAINT_AS_ACTIVE) ? IDR_THEME_TOOLBAR
                                        : IDR_THEME_TAB_BACKGROUND);

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), height());

  // Draw the top border.
  canvas->DrawBitmapInt(*(frame_edges.top_left), 0, 0);
  canvas->TileImageInt(
      *(frame_edges.top), frame_edges.top_left->width(), 0,
      width() - frame_edges.top_right->width(), frame_edges.top->height());
  canvas->DrawBitmapInt(
      *(frame_edges.top_right),
      width() - frame_edges.top_right->width(), 0);

  // Draw the right border.
  canvas->TileImageInt(
      *(frame_edges.right), width() - frame_edges.right->width(),
      frame_edges.top_right->height(), frame_edges.right->width(),
      height() - frame_edges.top_right->height() -
          frame_edges.bottom_right->height());

  // Draw the bottom border.
  canvas->DrawBitmapInt(
      *(frame_edges.bottom_right),
      width() - frame_edges.bottom_right->width(),
      height() - frame_edges.bottom_right->height());
  canvas->TileImageInt(
      *(frame_edges.bottom), frame_edges.bottom_left->width(),
      height() - frame_edges.bottom->height(),
      width() - frame_edges.bottom_left->width() -
          frame_edges.bottom_right->width(),
      frame_edges.bottom->height());
  canvas->DrawBitmapInt(
      *(frame_edges.bottom_left), 0,
      height() - frame_edges.bottom_left->height());

  // Draw the left border.
  canvas->TileImageInt(
      *(frame_edges.left), 0, frame_edges.top_left->height(),
      frame_edges.left->width(),
      height() - frame_edges.top_left->height() -
          frame_edges.bottom_left->height());
}

void PanelBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) {
  int client_area_top = client_view_bounds_.y();

  // Draw the top edge.
  int top_edge_y = client_area_top - client_edges.top->height();
  canvas->DrawBitmapInt(
      *(client_edges.top_left),
      client_view_bounds_.x() - client_edges.top_left->width(),
      top_edge_y);
  canvas->TileImageInt(
      *(client_edges.top), client_view_bounds_.x(), top_edge_y,
      client_view_bounds_.width(), client_edges.top->height());
  canvas->DrawBitmapInt(
      *(client_edges.top_right), client_view_bounds_.right(), top_edge_y);

  // Draw the right edge.
  int client_area_bottom =
      std::max(client_area_top, client_view_bounds_.bottom());
  int client_area_height = client_area_bottom - client_area_top;
  canvas->TileImageInt(
      *(client_edges.right), client_view_bounds_.right(), client_area_top,
      client_edges.right->width(), client_area_height);

  // Draw the bottom edge.
  canvas->DrawBitmapInt(
      *(client_edges.bottom_right), client_view_bounds_.right(),
      client_area_bottom);
  canvas->TileImageInt(
      *(client_edges.bottom), client_view_bounds_.x(), client_area_bottom,
      client_view_bounds_.width(), client_edges.bottom_right->height());
  canvas->DrawBitmapInt(
      *(client_edges.bottom_left),
      client_view_bounds_.x() - client_edges.bottom_left->width(),
      client_area_bottom);

  // Draw the left edge.
  canvas->TileImageInt(
      *(client_edges.left),
      client_view_bounds_.x() - client_edges.left->width(),
      client_area_top, client_edges.left->width(), client_area_height);
}

void PanelBrowserFrameView::UpdateTitleBar() {
  title_label_->SetText(
      frame_->window_delegate()->GetWindowTitle());
}

void PanelBrowserFrameView::OnActivationChanged(bool active) {
  SchedulePaint();
}

void PanelBrowserFrameView::CreateOrUpdateOptionsMenu() {
  bool rebuild_menu = CreateOrUpdateOptionsMenuItems();
  if (options_menu_.get()) {
    if (rebuild_menu)
      options_menu_->Rebuild();
  } else {
    options_menu_.reset(new views::Menu2(options_menu_contents_.get()));
  }
}

bool PanelBrowserFrameView::CreateOrUpdateOptionsMenuItems() {
  // Determines if we need to rebuild the menu items. The menu items might
  // be different if any of the following have been changed since the last
  // time:
  // 1) Multiple panel vs. single panel.
  // 2) All panels minimized or not.
  bool should_manipulate_all  =
      browser_view_->panel()->manager()->active_count() > 1;
  bool should_restore_all =
      browser_view_->panel()->manager()->AreAllMinimized();

  bool rebuild_menu = false;
  if (options_menu_contents_.get()) {
    bool manipulate_all = options_menu_contents_->GetItemCount() > 1;
    bool restore_all =
        options_menu_contents_->GetCommandIdAt(0) == COMMAND_RESTORE_ALL;

    if (manipulate_all == should_manipulate_all &&
        restore_all == should_restore_all) {
      return false;
    }

    rebuild_menu = true;
    options_menu_contents_->Clear();
  } else {
    options_menu_contents_.reset(new ui::SimpleMenuModel(this));
  }

  if (should_manipulate_all) {
    if (should_restore_all) {
      options_menu_contents_->AddItem(
          COMMAND_RESTORE_ALL,
          l10n_util::GetStringUTF16(IDS_PANEL_OPTIONS_MENU_RESTORE_ALL));
    } else {
      options_menu_contents_->AddItem(
          COMMAND_MINIMIZE_ALL,
          l10n_util::GetStringUTF16(IDS_PANEL_OPTIONS_MENU_MINIMIZE_ALL));
    }
    options_menu_contents_->AddItem(
        COMMAND_CLOSE_ALL,
        l10n_util::GetStringUTF16(IDS_PANEL_OPTIONS_MENU_CLOSE_ALL));
    options_menu_contents_->AddSeparator();
  }

  options_menu_contents_->AddItem(
      COMMAND_ABOUT, l10n_util::GetStringUTF16(IDS_PANEL_OPTIONS_MENU_ABOUT));

  return rebuild_menu;
}
