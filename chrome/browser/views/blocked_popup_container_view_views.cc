// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/blocked_popup_container_view_views.h"

#include <math.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/gfx/scrollbar_size.h"
#include "app/l10n_util.h"
#include "app/menus/simple_menu_model.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/screen.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

namespace {
// The minimal border around the edge of the notification.
const int kSmallPadding = 2;

// The background color of the blocked popup notification.
const SkColor kBackgroundColorTop = SkColorSetRGB(255, 242, 183);
const SkColor kBackgroundColorBottom = SkColorSetRGB(250, 230, 145);

// The border color of the blocked popup notification. This is the same as the
// border around the inside of the tab contents.
const SkColor kBorderColor = SkColorSetRGB(190, 205, 223);

// So that the MenuButton doesn't change its size as its text changes, during
// construction we feed it the strings it will be displaying, so it can set the
// max text width to the right value.  "99" should preallocate enough space for
// all numbers we'd show.
const int kWidestNumber = 99;

// Rounded corner radius (in pixels).
const int kBackgroundCornerRadius = 4;

// Rounded corner definition so the top corners are rounded, and the bottom are
// normal 90 degree angles.
const SkScalar kRoundedCornerRad[8] = {
  // Top left corner
  SkIntToScalar(kBackgroundCornerRadius),
  SkIntToScalar(kBackgroundCornerRadius),
  // Top right corner
  SkIntToScalar(kBackgroundCornerRadius),
  SkIntToScalar(kBackgroundCornerRadius),
  // Bottom right corner
  0,
  0,
  // Bottom left corner
  0,
  0
};

}  // namespace

#if defined(OS_WIN)

// BlockedPopupContainerViewWidget Win ----------------------------------------

class BlockedPopupContainerViewWidget : public views::WidgetWin {
 public:
  BlockedPopupContainerViewWidget(BlockedPopupContainerViewViews* container,
                                  gfx::NativeView parent);

  void SetBoundsAndShow(const gfx::Rect& bounds);
  void Show();
  void Hide();

  // Returns the size of our parent.
  gfx::Size GetParentSize();

 private:
  virtual void OnSize(UINT param, const CSize& size);

  BlockedPopupContainerViewViews* container_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainerViewWidget);
};

BlockedPopupContainerViewWidget::BlockedPopupContainerViewWidget(
    BlockedPopupContainerViewViews* container, gfx::NativeView parent)
    : container_(container) {
  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
  WidgetWin::Init(parent, gfx::Rect());
}

void BlockedPopupContainerViewWidget::SetBoundsAndShow(
    const gfx::Rect& bounds) {
  SetWindowPos(HWND_TOP, bounds.x(), bounds.y(), bounds.width(),
               bounds.height(), SWP_SHOWWINDOW);
}

void BlockedPopupContainerViewWidget::Show() {
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void BlockedPopupContainerViewWidget::Hide() {
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
}

gfx::Size BlockedPopupContainerViewWidget::GetParentSize() {
  HWND parent = GetParent();
  RECT client_rect;
  ::GetClientRect(parent, &client_rect);
  return gfx::Size(client_rect.right - client_rect.left,
                   client_rect.bottom - client_rect.top);
}

void BlockedPopupContainerViewWidget::OnSize(UINT param, const CSize& size) {
  container_->UpdateWidgetShape(this, gfx::Size(size.cx, size.cy));

  LayoutRootView();
}

#elif defined(OS_LINUX)

// BlockedPopupContainerViewWidget GTK ----------------------------------------
class BlockedPopupContainerViewWidget : public views::WidgetGtk {
 public:
  BlockedPopupContainerViewWidget(BlockedPopupContainerViewViews* container,
                                  gfx::NativeView parent);

  void SetBoundsAndShow(const gfx::Rect& bounds);

  // Returns the size of our parent.
  gfx::Size GetParentSize();

 private:
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);

  BlockedPopupContainerViewViews* container_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainerViewWidget);
};

BlockedPopupContainerViewWidget::BlockedPopupContainerViewWidget(
    BlockedPopupContainerViewViews* container,
    gfx::NativeView parent)
    : views::WidgetGtk(views::WidgetGtk::TYPE_CHILD),
      container_(container) {
  WidgetGtk::Init(parent, gfx::Rect());
}

void BlockedPopupContainerViewWidget::SetBoundsAndShow(
    const gfx::Rect& bounds) {
  SetBounds(bounds);
  Show();
}

gfx::Size BlockedPopupContainerViewWidget::GetParentSize() {
  GtkWidget* parent = gtk_widget_get_parent(GetNativeView());
  return gfx::Size(parent->allocation.width, parent->allocation.height);
}

void BlockedPopupContainerViewWidget::OnSizeAllocate(
    GtkWidget* widget, GtkAllocation* allocation) {
  WidgetGtk::OnSizeAllocate(widget, allocation);
  container_->UpdateWidgetShape(
      this, gfx::Size(allocation->width, allocation->height));
}

#endif

// BlockedPopupContainerInternalView ------------------------------------------

// The view presented to the user notifying them of the number of popups
// blocked. This view should only be used inside of BlockedPopupContainer.
class BlockedPopupContainerInternalView : public views::View,
                                  public views::ButtonListener,
                                  public menus::SimpleMenuModel::Delegate {
 public:
  explicit BlockedPopupContainerInternalView(
      BlockedPopupContainerViewViews* container);
  ~BlockedPopupContainerInternalView();

  // Sets the label on the menu button.
  void UpdateLabel();

  std::wstring label() const { return popup_count_label_->text(); }

  // Overridden from views::View:

  // Paints our border and background. (Does not paint children.)
  virtual void Paint(gfx::Canvas* canvas);
  // Sets positions of all child views.
  virtual void Layout();
  // Gets the desired size of the popup notification.
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from SimpleMenuModel::Delegate:

  // Displays the status of the "Show Blocked Popup Notification" item.
  virtual bool IsCommandIdChecked(int id) const;
  virtual bool IsCommandIdEnabled(int id) const { return true; }
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator) {
    return false;
  }
  // Called after user clicks a menu item.
  virtual void ExecuteCommand(int id);

 private:
  // Our owner and HWND parent.
  BlockedPopupContainerViewViews* container_;

  // The button which brings up the popup menu.
  views::MenuButton* popup_count_label_;

  // Our "X" button.
  views::ImageButton* close_button_;

  // Model for the menu.
  scoped_ptr<menus::SimpleMenuModel> launch_menu_model_;

  // Popup menu shown to user.
  scoped_ptr<views::Menu2> launch_menu_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainerInternalView);
};


BlockedPopupContainerInternalView::BlockedPopupContainerInternalView(
    BlockedPopupContainerViewViews* container)
    : container_(container) {
  ResourceBundle &resource_bundle = ResourceBundle::GetSharedInstance();

  // Create a button with a multidigit number to reserve space.
  popup_count_label_ = new views::MenuButton(
      this,
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                            IntToWString(kWidestNumber)),
      NULL, true);
  // Now set the text to the other possible display strings so that the button
  // will update its max text width (in case one of these string is longer).
  popup_count_label_->SetText(l10n_util::GetString(IDS_POPUPS_UNBLOCKED));
  popup_count_label_->SetText(l10n_util::GetString(IDS_BLOCKED_NOTICE_COUNT));
  popup_count_label_->set_alignment(views::TextButton::ALIGN_CENTER);
  AddChildView(popup_count_label_);

  // For now, we steal the Find close button, since it looks OK.
  close_button_ = new views::ImageButton(this);
  close_button_->SetFocusable(true);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR_P));
  AddChildView(close_button_);

  set_background(views::Background::CreateStandardPanelBackground());
  UpdateLabel();
}

BlockedPopupContainerInternalView::~BlockedPopupContainerInternalView() {
}

void BlockedPopupContainerInternalView::UpdateLabel() {
  size_t blocked_notices = container_->model()->GetBlockedNoticeCount();
  size_t blocked_items = container_->model()->GetBlockedPopupCount() +
      blocked_notices;

  std::wstring label;
  if (blocked_items == 0) {
    label = l10n_util::GetString(IDS_POPUPS_UNBLOCKED);
  } else if (blocked_notices == 0) {
    label = l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                                  UintToWString(blocked_items));
  } else {
    label = l10n_util::GetStringF(IDS_BLOCKED_NOTICE_COUNT,
                                  UintToWString(blocked_items));
  }
  popup_count_label_->SetText(label);

  Layout();
  SchedulePaint();
}

void BlockedPopupContainerInternalView::Paint(gfx::Canvas* canvas) {
  // Draw the standard background.
  View::Paint(canvas);

  SkRect rect;
  rect.set(0, 0, SkIntToScalar(width()), SkIntToScalar(height()));

  // Draw the border
  SkPaint border_paint;
  border_paint.setFlags(SkPaint::kAntiAlias_Flag);
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setColor(kBorderColor);
  SkPath border_path;
  border_path.addRoundRect(rect, kRoundedCornerRad, SkPath::kCW_Direction);
  canvas->drawPath(border_path, border_paint);
}

void BlockedPopupContainerInternalView::Layout() {
  gfx::Size panel_size = GetPreferredSize();
  gfx::Size button_size = close_button_->GetPreferredSize();
  gfx::Size size = popup_count_label_->GetPreferredSize();

  popup_count_label_->SetBounds(kSmallPadding, kSmallPadding,
                                size.width(),
                                size.height());

  int close_button_padding =
      static_cast<int>(ceil(panel_size.height() / 2.0) -
                       ceil(button_size.height() / 2.0));
  close_button_->SetBounds(width() - button_size.width() - close_button_padding,
                           close_button_padding,
                           button_size.width(),
                           button_size.height());
}

gfx::Size BlockedPopupContainerInternalView::GetPreferredSize() {
  gfx::Size preferred_size = popup_count_label_->GetPreferredSize();
  preferred_size.Enlarge(close_button_->GetPreferredSize().width(), 0);
  // Add padding to all sides of the |popup_count_label_| except the right.
  preferred_size.Enlarge(kSmallPadding, 2 * kSmallPadding);

  // Add padding to the left and right side of |close_button_| equal to its
  // horizontal/vertical spacing.
  gfx::Size button_size = close_button_->GetPreferredSize();
  int close_button_padding =
      static_cast<int>(ceil(preferred_size.height() / 2.0) -
                       ceil(button_size.height() / 2.0));
  preferred_size.Enlarge(2 * close_button_padding, 0);

  return preferred_size;
}

void BlockedPopupContainerInternalView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == close_button_) {
    container_->model()->set_dismissed();
    container_->model()->CloseAll();
  }

  if (sender != popup_count_label_)
    return;

  launch_menu_model_.reset(new menus::SimpleMenuModel(this));

  // Set items 1 .. popup_count as individual popups.
  size_t popup_count = container_->model()->GetBlockedPopupCount();
  for (size_t i = 0; i < popup_count; ++i) {
    std::wstring url, title;
    container_->GetURLAndTitleForPopup(i, &url, &title);
    // We can't just use the index into container_ here because Menu reserves
    // the value 0 as the nop command.
    launch_menu_model_->AddItem(i + 1,
        l10n_util::GetStringFUTF16(IDS_POPUP_TITLE_FORMAT, WideToUTF16(url),
                                   WideToUTF16(title)));
  }

  // Set items (kImpossibleNumberOfPopups + 1) ..
  // (kImpossibleNumberOfPopups + hosts.size()) as hosts.
  std::vector<std::string> hosts(container_->model()->GetHosts());
  if (!hosts.empty() && (popup_count > 0))
    launch_menu_model_->AddSeparator();
  size_t first_host = BlockedPopupContainer::kImpossibleNumberOfPopups + 1;
  for (size_t i = 0; i < hosts.size(); ++i) {
    launch_menu_model_->AddCheckItem(first_host + i,
        l10n_util::GetStringFUTF16(IDS_POPUP_HOST_FORMAT,
                                   UTF8ToUTF16(hosts[i])));
  }

  // Set items (kImpossibleNumberOfPopups + hosts.size() + 2) ..
  // (kImpossibleNumberOfPopups + hosts.size() + 1 + notice_count) as notices.
  size_t notice_count = container_->model()->GetBlockedNoticeCount();
  if (notice_count && (!hosts.empty() || (popup_count > 0)))
    launch_menu_model_->AddSeparator();
  size_t first_notice = first_host + hosts.size() + 1;
  for (size_t i = 0; i < notice_count; ++i) {
    std::string host;
    string16 reason;
    container_->model()->GetHostAndReasonForNotice(i, &host, &reason);
    launch_menu_model_->AddItem(first_notice + i,
        l10n_util::GetStringFUTF16(IDS_NOTICE_TITLE_FORMAT, ASCIIToUTF16(host),
                                   reason));
  }

  launch_menu_.reset(new views::Menu2(launch_menu_model_.get()));
  launch_menu_->RunContextMenuAt(views::Screen::GetCursorScreenPoint());
}

bool BlockedPopupContainerInternalView::IsCommandIdChecked(int id) const {
  // |id| should be > 0 since all index based commands have 1 added to them.
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);

  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    id_size_t -= BlockedPopupContainer::kImpossibleNumberOfPopups + 1;
    if (id_size_t < container_->model()->GetPopupHostCount())
      return container_->model()->IsHostWhitelisted(id_size_t);
  }

  return false;
}

void BlockedPopupContainerInternalView::ExecuteCommand(int id) {
  // |id| should be > 0 since all index based commands have 1 added to them.
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);

  // Is this a click on a popup?
  if (id_size_t < BlockedPopupContainer::kImpossibleNumberOfPopups) {
    container_->model()->LaunchPopupAtIndex(id_size_t - 1);
    return;
  }

  // |id| shouldn't be == kImpossibleNumberOfPopups since the popups end before
  // this and the hosts start after it.  (If it is used, it is as a separator.)
  DCHECK_NE(id_size_t, BlockedPopupContainer::kImpossibleNumberOfPopups);
  id_size_t -= BlockedPopupContainer::kImpossibleNumberOfPopups + 1;

  // Is this a click on a host?
  size_t host_count = container_->model()->GetPopupHostCount();
  if (id_size_t < host_count) {
    container_->model()->ToggleWhitelistingForHost(id_size_t);
    return;
  }

  // |id shouldn't be == host_count since this is the separator between hosts
  // and notices.
  DCHECK_NE(id_size_t, host_count);
  id_size_t -= host_count + 1;

  // Nothing to do for now for notices.
}

// BlockedPopupContainerViewViews ---------------------------------------------

// static
BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  return new BlockedPopupContainerViewViews(container);
}

BlockedPopupContainerViewViews::~BlockedPopupContainerViewViews() {
}

void BlockedPopupContainerViewViews::GetURLAndTitleForPopup(
    size_t index, std::wstring* url, std::wstring* title) const {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = model()->GetTabContentsAt(index);
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToWide(tab_contents_url.possibly_invalid_spec());
  *title = UTF16ToWideHack(tab_contents->GetTitle());
}

// Overridden from AnimationDelegate:

void BlockedPopupContainerViewViews::AnimationStarted(
    const Animation* animation) {
  SetPosition();
}

void BlockedPopupContainerViewViews::AnimationEnded(const Animation* animation) {
  SetPosition();
}

void BlockedPopupContainerViewViews::AnimationProgressed(
    const Animation* animation) {
  SetPosition();
}

// Overridden from BlockedPopupContainerView:

void BlockedPopupContainerViewViews::SetPosition() {
  gfx::Size parent_size = widget_->GetParentSize();

  // TODO(erg): There's no way to detect whether scroll bars are
  // visible, so for beta, we're just going to assume that the
  // vertical scroll bar is visible, and not care about covering up
  // the horizontal scroll bar. Fixing this is half of
  // http://b/1118139.
  gfx::Point anchor_point(parent_size.width() - gfx::scrollbar_size(),
                          parent_size.height());

  gfx::Size size = container_view_->GetPreferredSize();
  int base_x = anchor_point.x() - size.width();

  int real_height =
      static_cast<int>(size.height() * slide_animation_->GetCurrentValue());
  int real_y = anchor_point.y() - real_height;

  if (real_height > 0) {
    int x;
    if (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT) {
      // Size this window using the anchor point as top-right corner.
      x = base_x;
    } else {
      // Size this window to the bottom left corner of top client window. In
      // Chrome, scrollbars always appear on the right, even for a RTL page or
      // when the UI is RTL (see http://crbug.com/6113 for more detail). Thus 0
      // is always a safe value for x-axis.
      x = 0;
    }
    widget_->SetBoundsAndShow(gfx::Rect(x, real_y, size.width(), real_height));
    container_view_->SchedulePaint();
  } else {
    widget_->Hide();
  }
}

void BlockedPopupContainerViewViews::ShowView() {
  widget_->Show();
  slide_animation_->Show();
}

void BlockedPopupContainerViewViews::UpdateLabel() {
  container_view_->UpdateLabel();
}

void BlockedPopupContainerViewViews::HideView() {
  slide_animation_->Hide();
}

void BlockedPopupContainerViewViews::Destroy() {
  widget_->CloseNow();
  delete this;
}

// private:

BlockedPopupContainerViewViews::BlockedPopupContainerViewViews(
    BlockedPopupContainer* container)
    : widget_(NULL),
      model_(container),
      container_view_(NULL),
      slide_animation_(new SlideAnimation(this)) {
  widget_ = new BlockedPopupContainerViewWidget(this,
      model_->GetConstrainingContents(NULL)->GetNativeView());
  container_view_ = new BlockedPopupContainerInternalView(this);
  widget_->SetContentsView(container_view_);
  SetPosition();
}

void BlockedPopupContainerViewViews::UpdateWidgetShape(
    BlockedPopupContainerViewWidget* widget, const gfx::Size& size) {
  // Set the shape so we have rounded corners on the top.
  SkRect rect;
  rect.set(0, 0, SkIntToScalar(size.width()), SkIntToScalar(size.height()));
  gfx::Path path;
  path.addRoundRect(rect, kRoundedCornerRad, SkPath::kCW_Direction);
  widget->SetShape(path.CreateNativeRegion());
}
