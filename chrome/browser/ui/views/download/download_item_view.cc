// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_item_view.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "content/browser/download/download_state_info.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "unicode/uchar.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"

// TODO(paulg): These may need to be adjusted when download progress
//              animation is added, and also possibly to take into account
//              different screen resolutions.
static const int kTextWidth = 140;            // Pixels
static const int kDangerousTextWidth = 200;   // Pixels
static const int kHorizontalTextPadding = 2;  // Pixels
static const int kVerticalPadding = 3;        // Pixels
static const int kVerticalTextSpacer = 2;     // Pixels
static const int kVerticalTextPadding = 2;    // Pixels

// The maximum number of characters we show in a file name when displaying the
// dangerous download message.
static const int kFileNameMaxLength = 20;

// We add some padding before the left image so that the progress animation icon
// hides the corners of the left image.
static const int kLeftPadding = 0;  // Pixels.

// The space between the Save and Discard buttons when prompting for a dangerous
// download.
static const int kButtonPadding = 5;  // Pixels.

// The space on the left and right side of the dangerous download label.
static const int kLabelPadding = 4;  // Pixels.

static const SkColor kFileNameDisabledColor = SkColorSetRGB(171, 192, 212);

// How long the 'download complete' animation should last for.
static const int kCompleteAnimationDurationMs = 2500;

// How long the 'download interrupted' animation should last for.
static const int kInterruptedAnimationDurationMs = 2500;

// How long we keep the item disabled after the user clicked it to open the
// downloaded item.
static const int kDisabledOnOpenDuration = 3000;

// Darken light-on-dark download status text by 20% before drawing, thus
// creating a "muted" version of title text for both dark-on-light and
// light-on-dark themes.
static const double kDownloadItemLuminanceMod = 0.8;

DownloadItemView::DownloadItemView(DownloadItem* download,
    DownloadShelfView* parent,
    BaseDownloadItemModel* model)
  : warning_icon_(NULL),
    download_(download),
    parent_(parent),
    status_text_(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING)),
    body_state_(NORMAL),
    drop_down_state_(NORMAL),
    progress_angle_(download_util::kStartAngleDegrees),
    drop_down_pressed_(false),
    dragging_(false),
    starting_drag_(false),
    model_(model),
    save_button_(NULL),
    discard_button_(NULL),
    dangerous_download_label_(NULL),
    dangerous_download_label_sized_(false),
    disabled_while_opening_(false),
    creation_time_(base::Time::Now()),
    ALLOW_THIS_IN_INITIALIZER_LIST(reenable_method_factory_(this)) {
  DCHECK(download_);
  download_->AddObserver(this);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  BodyImageSet normal_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM)
  };
  normal_body_image_set_ = normal_body_image_set;

  DropDownImageSet normal_drop_down_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM)
  };
  normal_drop_down_image_set_ = normal_drop_down_image_set;

  BodyImageSet hot_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H)
  };
  hot_body_image_set_ = hot_body_image_set;

  DropDownImageSet hot_drop_down_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H)
  };
  hot_drop_down_image_set_ = hot_drop_down_image_set;

  BodyImageSet pushed_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P)
  };
  pushed_body_image_set_ = pushed_body_image_set;

  DropDownImageSet pushed_drop_down_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P)
  };
  pushed_drop_down_image_set_ = pushed_drop_down_image_set;

  BodyImageSet dangerous_mode_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_NO_DD),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_NO_DD),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_NO_DD)
  };
  dangerous_mode_body_image_set_ = dangerous_mode_body_image_set;

  LoadIcon();
  tooltip_text_ = download_->GetFileNameToReportUser().LossyDisplayName();

  font_ = ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
  box_height_ = std::max<int>(2 * kVerticalPadding + font_.GetHeight() +
                                  kVerticalTextPadding + font_.GetHeight(),
                              2 * kVerticalPadding +
                                  normal_body_image_set_.top_left->height() +
                                  normal_body_image_set_.bottom_left->height());

  if (download_util::kSmallProgressIconSize > box_height_)
    box_y_ = (download_util::kSmallProgressIconSize - box_height_) / 2;
  else
    box_y_ = kVerticalPadding;

  gfx::Size size = GetPreferredSize();
  if (base::i18n::IsRTL()) {
    // Drop down button is glued to the left of the download shelf.
    drop_down_x_left_ = 0;
    drop_down_x_right_ = normal_drop_down_image_set_.top->width();
  } else {
    // Drop down button is glued to the right of the download shelf.
    drop_down_x_left_ =
        size.width() - normal_drop_down_image_set_.top->width();
    drop_down_x_right_ = size.width();
  }

  body_hover_animation_.reset(new ui::SlideAnimation(this));
  drop_hover_animation_.reset(new ui::SlideAnimation(this));

  if (download->GetSafetyState() == DownloadItem::DANGEROUS)
    EnterDangerousMode();

  UpdateAccessibleName();
  set_accessibility_focusable(true);

  // Set up our animation.
  StartDownloadProgress();
}

DownloadItemView::~DownloadItemView() {
  if (context_menu_.get())
    context_menu_->Stop();
  icon_consumer_.CancelAllRequests();
  StopDownloadProgress();
  download_->RemoveObserver(this);
}

// Progress animation handlers.

void DownloadItemView::UpdateDownloadProgress() {
  progress_angle_ = (progress_angle_ +
                     download_util::kUnknownIncrementDegrees) %
                    download_util::kMaxDegrees;
  SchedulePaint();
}

void DownloadItemView::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(download_util::kProgressRateMs), this,
      &DownloadItemView::UpdateDownloadProgress);
}

void DownloadItemView::StopDownloadProgress() {
  progress_timer_.Stop();
}

void DownloadItemView::OnExtractIconComplete(IconManager::Handle handle,
                                             gfx::Image* icon_bitmap) {
  if (icon_bitmap)
    parent()->SchedulePaint();
}

// DownloadObserver interface.

// Update the progress graphic on the icon and our text status label
// to reflect our current bytes downloaded, time remaining.
void DownloadItemView::OnDownloadUpdated(DownloadItem* download) {
  DCHECK(download == download_);

  if (body_state_ == DANGEROUS &&
      download->GetSafetyState() == DownloadItem::DANGEROUS_BUT_VALIDATED) {
    // We have been approved.
    ClearDangerousMode();
  } else if (body_state_ != DANGEROUS &&
      download->GetSafetyState() == DownloadItem::DANGEROUS) {
    EnterDangerousMode();
    // Force the shelf to layout again as our size has changed.
    parent_->Layout();
    SchedulePaint();
  } else {
    string16 status_text = model_->GetStatusText();
    switch (download_->GetState()) {
      case DownloadItem::IN_PROGRESS:
        download_->IsPaused() ?
            StopDownloadProgress() : StartDownloadProgress();
        LoadIconIfItemPathChanged();
        break;
      case DownloadItem::INTERRUPTED:
        StopDownloadProgress();
        complete_animation_.reset(new ui::SlideAnimation(this));
        complete_animation_->SetSlideDuration(kInterruptedAnimationDurationMs);
        complete_animation_->SetTweenType(ui::Tween::LINEAR);
        complete_animation_->Show();
        SchedulePaint();
        LoadIcon();
        break;
      case DownloadItem::COMPLETE:
        if (download_->GetAutoOpened()) {
          parent_->RemoveDownloadView(this);  // This will delete us!
          return;
        }
        StopDownloadProgress();
        complete_animation_.reset(new ui::SlideAnimation(this));
        complete_animation_->SetSlideDuration(kCompleteAnimationDurationMs);
        complete_animation_->SetTweenType(ui::Tween::LINEAR);
        complete_animation_->Show();
        SchedulePaint();
        LoadIcon();
        break;
      case DownloadItem::CANCELLED:
        StopDownloadProgress();
        LoadIcon();
        break;
      case DownloadItem::REMOVING:
        parent_->RemoveDownloadView(this);  // This will delete us!
        return;
      default:
        NOTREACHED();
    }
    status_text_ = status_text;
  }
  UpdateAccessibleName();

  // We use the parent's (DownloadShelfView's) SchedulePaint, since there
  // are spaces between each DownloadItemView that the parent is responsible
  // for painting.
  parent()->SchedulePaint();
}

void DownloadItemView::OnDownloadOpened(DownloadItem* download) {
  disabled_while_opening_ = true;
  SetEnabled(false);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DownloadItemView::Reenable,
                 reenable_method_factory_.GetWeakPtr()),
      kDisabledOnOpenDuration);

  // Notify our parent.
  parent_->OpenedDownload(this);
}

// View overrides

// In dangerous mode we have to layout our buttons.
void DownloadItemView::Layout() {
  if (IsDangerousMode()) {
    dangerous_download_label_->SetEnabledColor(
      GetThemeProvider()->GetColor(ThemeService::COLOR_BOOKMARK_TEXT));

    int x = kLeftPadding + dangerous_mode_body_image_set_.top_left->width() +
      warning_icon_->width() + kLabelPadding;
    int y = (height() - dangerous_download_label_->height()) / 2;
    dangerous_download_label_->SetBounds(x, y,
                                         dangerous_download_label_->width(),
                                         dangerous_download_label_->height());
    gfx::Size button_size = GetButtonSize();
    x += dangerous_download_label_->width() + kLabelPadding;
    y = (height() - button_size.height()) / 2;
    save_button_->SetBounds(x, y, button_size.width(), button_size.height());
    x += button_size.width() + kButtonPadding;
    discard_button_->SetBounds(x, y, button_size.width(), button_size.height());
  }
}

gfx::Size DownloadItemView::GetPreferredSize() {
  int width, height;

  // First, we set the height to the height of two rows or text plus margins.
  height = 2 * kVerticalPadding + 2 * font_.GetHeight() + kVerticalTextPadding;
  // Then we increase the size if the progress icon doesn't fit.
  height = std::max<int>(height, download_util::kSmallProgressIconSize);

  if (IsDangerousMode()) {
    width = kLeftPadding + dangerous_mode_body_image_set_.top_left->width();
    width += warning_icon_->width() + kLabelPadding;
    width += dangerous_download_label_->width() + kLabelPadding;
    gfx::Size button_size = GetButtonSize();
    // Make sure the button fits.
    height = std::max<int>(height, 2 * kVerticalPadding + button_size.height());
    // Then we make sure the warning icon fits.
    height = std::max<int>(height, 2 * kVerticalPadding +
                                   warning_icon_->height());
    width += button_size.width() * 2 + kButtonPadding;
    width += dangerous_mode_body_image_set_.top_right->width();
  } else {
    width = kLeftPadding + normal_body_image_set_.top_left->width();
    width += download_util::kSmallProgressIconSize;
    width += kTextWidth;
    width += normal_body_image_set_.top_right->width();
    width += normal_drop_down_image_set_.top->width();
  }
  return gfx::Size(width, height);
}

// Handle a mouse click and open the context menu if the mouse is
// over the drop-down region.
bool DownloadItemView::OnMousePressed(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return true;

  // Stop any completion animation.
  if (complete_animation_.get() && complete_animation_->is_animating())
    complete_animation_->End();

  if (event.IsOnlyLeftMouseButton()) {
    if (InDropDownButtonXCoordinateRange(event.x())) {
      drop_down_pressed_ = true;
      SetState(NORMAL, PUSHED);
      // We are setting is_mouse_gesture to false when calling ShowContextMenu
      // so that the positioning of the context menu will be similar to a
      // keyboard invocation.  I.e. we want the menu to always be positioned
      // next to the drop down button instead of the next to the pointer.
      ShowContextMenu(event.location(), false);
    } else {
      SetState(PUSHED, NORMAL);
    }
  }
  return true;
}

// Handle drag (file copy) operations.
bool DownloadItemView::OnMouseDragged(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return true;

  if (!starting_drag_) {
    starting_drag_ = true;
    drag_start_point_ = event.location();
  }
  if (dragging_) {
    if (download_->IsComplete()) {
      IconManager* im = g_browser_process->icon_manager();
      gfx::Image* icon = im->LookupIcon(download_->GetUserVerifiedFilePath(),
                                        IconLoader::SMALL);
      if (icon) {
        views::Widget* widget = GetWidget();
        download_util::DragDownload(download_, icon,
                                    widget ? widget->GetNativeView() : NULL);
      }
    }
  } else if (ExceededDragThreshold(
                 event.location().x() - drag_start_point_.x(),
                 event.location().y() - drag_start_point_.y())) {
    dragging_ = true;
  }
  return true;
}

void DownloadItemView::OnMouseReleased(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  if (event.IsOnlyLeftMouseButton() &&
      !InDropDownButtonXCoordinateRange(event.x())) {
    OpenDownload();
  }

  SetState(NORMAL, NORMAL);
}

void DownloadItemView::OnMouseCaptureLost() {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    starting_drag_ = false;
  } else {
    SetState(NORMAL, NORMAL);
  }
}

void DownloadItemView::OnMouseMoved(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  bool on_body = !InDropDownButtonXCoordinateRange(event.x());
  SetState(on_body ? HOT : NORMAL, on_body ? NORMAL : HOT);
  if (on_body) {
    body_hover_animation_->Show();
    drop_hover_animation_->Hide();
  } else {
    body_hover_animation_->Hide();
    drop_hover_animation_->Show();
  }
}

void DownloadItemView::OnMouseExited(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  SetState(NORMAL, drop_down_pressed_ ? PUSHED : NORMAL);
  body_hover_animation_->Hide();
  drop_hover_animation_->Hide();
}

bool DownloadItemView::OnKeyPressed(const views::KeyEvent& event) {
  // Key press should not activate us in dangerous mode.
  if (IsDangerousMode())
    return true;

  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    OpenDownload();
    return true;
  }
  return false;
}

bool DownloadItemView::GetTooltipText(const gfx::Point& p,
                                      string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  tooltip->assign(tooltip_text_);
  return true;
}

void DownloadItemView::ShowContextMenu(const gfx::Point& p,
                                       bool is_mouse_gesture) {
  gfx::Point point = p;
  gfx::Size size;

  // Similar hack as in MenuButton.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to NULL.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())->
      SetMouseHandler(NULL);

  // If |is_mouse_gesture| is false, |p| is ignored. The menu is shown aligned
  // to drop down arrow button.
  if (!is_mouse_gesture) {
    drop_down_pressed_ = true;
    SetState(NORMAL, PUSHED);
    point.SetPoint(drop_down_x_left_, box_y_);
    size.SetSize(drop_down_x_right_ - drop_down_x_left_, box_height_);
  }

  views::View::ConvertPointToScreen(this, &point);

  if (!context_menu_.get())
    context_menu_.reset(new DownloadShelfContextMenuView(model_.get()));
  // When we call the Run method on the menu, it runs an inner message loop
  // that might causes us to be deleted.
  if (context_menu_->Run(GetWidget()->GetTopLevelWidget(),
                         gfx::Rect(point, size)))
    return;  // We have been deleted! Don't access 'this'.

  // If the menu action was to remove the download, this view will also be
  // invalid so we must not access 'this' in this case.
  if (context_menu_->download_item()) {
    drop_down_pressed_ = false;
    // Showing the menu blocks. Here we revert the state.
    SetState(NORMAL, NORMAL);
  }
}

void DownloadItemView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = accessible_name_;
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  if (download_->GetSafetyState() == DownloadItem::DANGEROUS) {
    state->state = ui::AccessibilityTypes::STATE_UNAVAILABLE;
  } else {
    state->state = ui::AccessibilityTypes::STATE_HASPOPUP;
  }
}

void DownloadItemView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == discard_button_) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                             base::Time::Now() - creation_time_);
    if (download_->IsPartialDownload())
      download_->Cancel(true);
    download_->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
    // WARNING: we are deleted at this point.  Don't access 'this'.
  } else if (sender == save_button_) {
    // The user has confirmed a dangerous download.  We'd record how quickly the
    // user did this to detect whether we're being clickjacked.
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download",
                             base::Time::Now() - creation_time_);
    // This will change the state and notify us.
    download_->DangerousDownloadValidated();
  }
}

void DownloadItemView::AnimationProgressed(const ui::Animation* animation) {
  // We don't care if what animation (body button/drop button/complete),
  // is calling back, as they all have to go through the same paint call.
  SchedulePaint();
}

void DownloadItemView::OnPaint(gfx::Canvas* canvas) {
  BodyImageSet* body_image_set = NULL;
  switch (body_state_) {
    case NORMAL:
    case HOT:
      body_image_set = &normal_body_image_set_;
      break;
    case PUSHED:
      body_image_set = &pushed_body_image_set_;
      break;
    case DANGEROUS:
      body_image_set = &dangerous_mode_body_image_set_;
      break;
    default:
      NOTREACHED();
  }
  DropDownImageSet* drop_down_image_set = NULL;
  switch (drop_down_state_) {
    case NORMAL:
    case HOT:
      drop_down_image_set = &normal_drop_down_image_set_;
      break;
    case PUSHED:
      drop_down_image_set = &pushed_drop_down_image_set_;
      break;
    case DANGEROUS:
      drop_down_image_set = NULL;  // No drop-down in dangerous mode.
      break;
    default:
      NOTREACHED();
  }

  int center_width = width() - kLeftPadding -
                     body_image_set->left->width() -
                     body_image_set->right->width() -
                     (drop_down_image_set ?
                        normal_drop_down_image_set_.center->width() :
                        0);

  // May be caused by animation.
  if (center_width <= 0)
    return;

  // Draw status before button image to effectively lighten text.
  if (!IsDangerousMode()) {
    if (!status_text_.empty()) {
      int mirrored_x = GetMirroredXWithWidthInView(
          download_util::kSmallProgressIconSize, kTextWidth);
      // Add font_.height() to compensate for title, which is drawn later.
      int y = box_y_ + kVerticalPadding + font_.GetHeight() +
              kVerticalTextPadding;
      SkColor file_name_color = GetThemeProvider()->GetColor(
          ThemeService::COLOR_BOOKMARK_TEXT);
      // If text is light-on-dark, lightening it alone will do nothing.
      // Therefore we mute luminance a wee bit before drawing in this case.
      if (color_utils::RelativeLuminance(file_name_color) > 0.5)
          file_name_color = SkColorSetRGB(
              static_cast<int>(kDownloadItemLuminanceMod *
                               SkColorGetR(file_name_color)),
              static_cast<int>(kDownloadItemLuminanceMod *
                               SkColorGetG(file_name_color)),
              static_cast<int>(kDownloadItemLuminanceMod *
                               SkColorGetB(file_name_color)));
      canvas->DrawStringInt(status_text_, font_,
                            file_name_color, mirrored_x, y, kTextWidth,
                            font_.GetHeight());
    }
  }

  // Paint the background images.
  int x = kLeftPadding;
  canvas->Save();
  if (base::i18n::IsRTL()) {
    // Since we do not have the mirrored images for
    // (hot_)body_image_set->top_left, (hot_)body_image_set->left,
    // (hot_)body_image_set->bottom_left, and drop_down_image_set,
    // for RTL UI, we flip the canvas to draw those images mirrored.
    // Consequently, we do not need to mirror the x-axis of those images.
    canvas->Translate(gfx::Point(width(), 0));
    canvas->Scale(-1, 1);
  }
  PaintBitmaps(canvas,
               body_image_set->top_left, body_image_set->left,
               body_image_set->bottom_left,
               x, box_y_, box_height_, body_image_set->top_left->width());
  x += body_image_set->top_left->width();
  PaintBitmaps(canvas,
               body_image_set->top, body_image_set->center,
               body_image_set->bottom,
               x, box_y_, box_height_, center_width);
  x += center_width;
  PaintBitmaps(canvas,
               body_image_set->top_right, body_image_set->right,
               body_image_set->bottom_right,
               x, box_y_, box_height_, body_image_set->top_right->width());

  // Overlay our body hot state.
  if (body_hover_animation_->GetCurrentValue() > 0) {
    canvas->SaveLayerAlpha(
        static_cast<int>(body_hover_animation_->GetCurrentValue() * 255));
    canvas->GetSkCanvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);

    int x = kLeftPadding;
    PaintBitmaps(canvas,
                 hot_body_image_set_.top_left, hot_body_image_set_.left,
                 hot_body_image_set_.bottom_left,
                 x, box_y_, box_height_, hot_body_image_set_.top_left->width());
    x += body_image_set->top_left->width();
    PaintBitmaps(canvas,
                 hot_body_image_set_.top, hot_body_image_set_.center,
                 hot_body_image_set_.bottom,
                 x, box_y_, box_height_, center_width);
    x += center_width;
    PaintBitmaps(canvas,
                 hot_body_image_set_.top_right, hot_body_image_set_.right,
                 hot_body_image_set_.bottom_right,
                 x, box_y_, box_height_,
                 hot_body_image_set_.top_right->width());
    canvas->Restore();
  }

  x += body_image_set->top_right->width();

  // Paint the drop-down.
  if (drop_down_image_set) {
    PaintBitmaps(canvas,
                 drop_down_image_set->top, drop_down_image_set->center,
                 drop_down_image_set->bottom,
                 x, box_y_, box_height_, drop_down_image_set->top->width());

    // Overlay our drop-down hot state.
    if (drop_hover_animation_->GetCurrentValue() > 0) {
      canvas->SaveLayerAlpha(
          static_cast<int>(drop_hover_animation_->GetCurrentValue() * 255));
      canvas->GetSkCanvas()->drawARGB(0, 255, 255, 255,
                                      SkXfermode::kClear_Mode);

      PaintBitmaps(canvas,
                   drop_down_image_set->top, drop_down_image_set->center,
                   drop_down_image_set->bottom,
                   x, box_y_, box_height_, drop_down_image_set->top->width());

      canvas->Restore();
    }
  }

  // Restore the canvas to avoid file name etc. text are drawn flipped.
  // Consequently, the x-axis of following canvas->DrawXXX() method should be
  // mirrored so the text and images are down in the right positions.
  canvas->Restore();

  // Print the text, left aligned and always print the file extension.
  // Last value of x was the end of the right image, just before the button.
  // Note that in dangerous mode we use a label (as the text is multi-line).
  if (!IsDangerousMode()) {
    string16 filename;
    if (!disabled_while_opening_) {
      filename = ui::ElideFilename(download_->GetFileNameToReportUser(),
                                   font_, kTextWidth);
    } else {
      // First, Calculate the download status opening string width.
      string16 status_string =
          l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING, string16());
      int status_string_width = font_.GetStringWidth(status_string);
      // Then, elide the file name.
      string16 filename_string =
          ui::ElideFilename(download_->GetFileNameToReportUser(), font_,
                            kTextWidth - status_string_width);
      // Last, concat the whole string.
      filename = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING,
                                            filename_string);
    }

    int mirrored_x = GetMirroredXWithWidthInView(
        download_util::kSmallProgressIconSize, kTextWidth);
    SkColor file_name_color = GetThemeProvider()->GetColor(
        ThemeService::COLOR_BOOKMARK_TEXT);
    int y =
        box_y_ + (status_text_.empty() ?
                  ((box_height_ - font_.GetHeight()) / 2) : kVerticalPadding);

    // Draw the file's name.
    canvas->DrawStringInt(filename, font_,
                          IsEnabled() ? file_name_color :
                                        kFileNameDisabledColor,
                          mirrored_x, y, kTextWidth, font_.GetHeight());
  }

  // Load the icon.
  IconManager* im = g_browser_process->icon_manager();
  gfx::Image* image = im->LookupIcon(download_->GetUserVerifiedFilePath(),
                                     IconLoader::SMALL);
  const SkBitmap* icon = NULL;
  if (IsDangerousMode())
    icon = warning_icon_;
  else if (image)
    icon = *image;

  // We count on the fact that the icon manager will cache the icons and if one
  // is available, it will be cached here. We *don't* want to request the icon
  // to be loaded here, since this will also get called if the icon can't be
  // loaded, in which case LookupIcon will always be NULL. The loading will be
  // triggered only when we think the status might change.
  if (icon) {
    if (!IsDangerousMode()) {
      if (download_->IsInProgress()) {
        download_util::PaintDownloadProgress(canvas, this, 0, 0,
                                             progress_angle_,
                                             download_->PercentComplete(),
                                             download_util::SMALL);
      } else if (download_->IsComplete() &&
                 complete_animation_.get() &&
                 complete_animation_->is_animating()) {
        if (download_->IsInterrupted()) {
          download_util::PaintDownloadInterrupted(canvas, this, 0, 0,
              complete_animation_->GetCurrentValue(),
              download_util::SMALL);
        } else {
          download_util::PaintDownloadComplete(canvas, this, 0, 0,
              complete_animation_->GetCurrentValue(),
              download_util::SMALL);
        }
      }
    }

    // Draw the icon image.
    int icon_x, icon_y;

    if (IsDangerousMode()) {
      icon_x = kLeftPadding + body_image_set->top_left->width();
      icon_y = (height() - icon->height()) / 2;
    } else {
      icon_x = download_util::kSmallProgressIconOffset;
      icon_y = download_util::kSmallProgressIconOffset;
    }
    icon_x = GetMirroredXWithWidthInView(icon_x, icon->width());
    if (IsEnabled()) {
      canvas->DrawBitmapInt(*icon, icon_x, icon_y);
    } else {
      // Use an alpha to make the image look disabled.
      SkPaint paint;
      paint.setAlpha(120);
      canvas->DrawBitmapInt(*icon, icon_x, icon_y, paint);
    }
  }
}

void DownloadItemView::OpenDownload() {
  // We're interested in how long it takes users to open downloads.  If they
  // open downloads super quickly, we should be concerned about clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - creation_time_);
  download_->OpenDownload();
  UpdateAccessibleName();
}

void DownloadItemView::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  last_download_item_path_ = download_->GetUserVerifiedFilePath();
  im->LoadIcon(last_download_item_path_,
               IconLoader::SMALL, &icon_consumer_,
               base::Bind(&DownloadItemView::OnExtractIconComplete,
                          base::Unretained(this)));
}

void DownloadItemView::LoadIconIfItemPathChanged() {
  FilePath current_download_path = download_->GetUserVerifiedFilePath();
  if (last_download_item_path_ == current_download_path)
    return;

  LoadIcon();
}

// Load an icon for the file type we're downloading, and animate any in progress
// download state.
void DownloadItemView::PaintBitmaps(gfx::Canvas* canvas,
                                    const SkBitmap* top_bitmap,
                                    const SkBitmap* center_bitmap,
                                    const SkBitmap* bottom_bitmap,
                                    int x, int y, int height, int width) {
  int middle_height = height - top_bitmap->height() - bottom_bitmap->height();
  // Draw the top.
  canvas->DrawBitmapInt(*top_bitmap,
                        0, 0, top_bitmap->width(), top_bitmap->height(),
                        x, y, width, top_bitmap->height(), false);
  y += top_bitmap->height();
  // Draw the center.
  canvas->DrawBitmapInt(*center_bitmap,
                        0, 0, center_bitmap->width(), center_bitmap->height(),
                        x, y, width, middle_height, false);
  y += middle_height;
  // Draw the bottom.
  canvas->DrawBitmapInt(*bottom_bitmap,
                        0, 0, bottom_bitmap->width(), bottom_bitmap->height(),
                        x, y, width, bottom_bitmap->height(), false);
}

void DownloadItemView::SetState(State body_state, State drop_down_state) {
  if (body_state_ == body_state && drop_down_state_ == drop_down_state)
    return;

  body_state_ = body_state;
  drop_down_state_ = drop_down_state;
  SchedulePaint();
}

void DownloadItemView::ClearDangerousMode() {
  DCHECK(download_->GetSafetyState() == DownloadItem::DANGEROUS_BUT_VALIDATED &&
         body_state_ == DANGEROUS && drop_down_state_ == DANGEROUS);

  body_state_ = NORMAL;
  drop_down_state_ = NORMAL;

  // Remove the views used by the dangerous mode.
  RemoveChildView(save_button_);
  delete save_button_;
  save_button_ = NULL;
  RemoveChildView(discard_button_);
  delete discard_button_;
  discard_button_ = NULL;
  RemoveChildView(dangerous_download_label_);
  delete dangerous_download_label_;
  dangerous_download_label_ = NULL;
  dangerous_download_label_sized_ = false;

  // Set the accessible name back to the status and filename instead of the
  // download warning.
  UpdateAccessibleName();

  // We need to load the icon now that the download_ has the real path.
  LoadIcon();
  tooltip_text_ = download_->GetFileNameToReportUser().LossyDisplayName();

  // Force the shelf to layout again as our size has changed.
  parent_->Layout();
  parent_->SchedulePaint();
}

void DownloadItemView::EnterDangerousMode() {
  DCHECK(body_state_ != DANGEROUS && drop_down_state_ != DANGEROUS);
  tooltip_text_.clear();
  body_state_ = DANGEROUS;
  drop_down_state_ = DANGEROUS;
  save_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(
           ChromeDownloadManagerDelegate::IsExtensionDownload(download_) ?
              IDS_CONTINUE_EXTENSION_DOWNLOAD : IDS_CONFIRM_DOWNLOAD));
  save_button_->set_ignore_minimum_size(true);
  discard_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_DISCARD_DOWNLOAD));
  discard_button_->set_ignore_minimum_size(true);
  AddChildView(save_button_);
  AddChildView(discard_button_);

  // Ensure the file name is not too long.

  // Extract the file extension (if any).
  FilePath filename(download_->GetTargetName());
#if defined(OS_POSIX)
  string16 extension = WideToUTF16(base::SysNativeMBToWide(
      filename.Extension()));
#else
  string16 extension = filename.Extension();
#endif

  // Remove leading '.'
  if (extension.length() > 0)
    extension = extension.substr(1);
#if defined(OS_POSIX)
  string16 rootname = WideToUTF16(base::SysNativeMBToWide(
      filename.RemoveExtension().value()));
#else
  string16 rootname = filename.RemoveExtension().value();
#endif

  // Elide giant extensions (this shouldn't currently be hit, but might
  // in future, should we ever notice unsafe giant extensions).
  if (extension.length() > kFileNameMaxLength / 2)
    ui::ElideString(extension, kFileNameMaxLength / 2, &extension);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // The dangerous download label text and icon are different
  // under different cases.
  string16 dangerous_label;
  if (download_->GetDangerType() == DownloadStateInfo::DANGEROUS_URL ||
    download_->GetDangerType() == DownloadStateInfo::DANGEROUS_CONTENT) {
    // TODO(noelutz): add the target filename to the warning message in the
    // case of a dangerous content warning.
    // Safebrowsing shows the download URL or content leads to malicious file.
    warning_icon_ = rb.GetBitmapNamed(IDR_SAFEBROWSING_WARNING);
    dangerous_label =
        l10n_util::GetStringUTF16(IDS_PROMPT_UNSAFE_DOWNLOAD_URL);
  } else {
    // The download file has dangerous file type (e.g.: an executable).
    DCHECK(download_->GetDangerType() == DownloadStateInfo::DANGEROUS_FILE);
    warning_icon_ = rb.GetBitmapNamed(IDR_WARNING);
    if (ChromeDownloadManagerDelegate::IsExtensionDownload(download_)) {
      dangerous_label =
          l10n_util::GetStringUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
    } else {
      ui::ElideString(rootname,
                      kFileNameMaxLength - extension.length(),
                      &rootname);
      string16 filename = rootname + ASCIIToUTF16(".") + extension;
      filename = base::i18n::GetDisplayStringInLTRDirectionality(filename);
      dangerous_label =
          l10n_util::GetStringFUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD, filename);
    }
  }

  dangerous_download_label_ = new views::Label(dangerous_label);
  dangerous_download_label_->SetMultiLine(true);
  dangerous_download_label_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  dangerous_download_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(dangerous_download_label_);
  SizeLabelToMinWidth();
}

gfx::Size DownloadItemView::GetButtonSize() {
  DCHECK(save_button_ && discard_button_);
  gfx::Size size;

  // We cache the size when successfully retrieved, not for performance reasons
  // but because if this DownloadItemView is being animated while the tab is
  // not showing, the native buttons are not parented and their preferred size
  // is 0, messing-up the layout.
  if (cached_button_size_.width() != 0)
    return cached_button_size_;

  size = save_button_->GetMinimumSize();
  gfx::Size discard_size = discard_button_->GetMinimumSize();

  size.SetSize(std::max(size.width(), discard_size.width()),
               std::max(size.height(), discard_size.height()));

  if (size.width() != 0)
    cached_button_size_ = size;

  return size;
}

// This method computes the minimum width of the label for displaying its text
// on 2 lines.  It just breaks the string in 2 lines on the spaces and keeps the
// configuration with minimum width.
void DownloadItemView::SizeLabelToMinWidth() {
  if (dangerous_download_label_sized_)
    return;

  string16 text = dangerous_download_label_->GetText();
  TrimWhitespace(text, TRIM_ALL, &text);
  DCHECK_EQ(string16::npos, text.find('\n'));

  // Make the label big so that GetPreferredSize() is not constrained by the
  // current width.
  dangerous_download_label_->SetBounds(0, 0, 1000, 1000);

  gfx::Size size;
  int min_width = -1;
  // Using BREAK_WORD can work in most cases, but it can also break
  // lines where it should not. Using BREAK_LINE is safer although
  // slower for Chinese/Japanese. This is not perf-critical at all, though.
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_LINE);
  bool status = iter.Init();
  DCHECK(status);

  string16 current_text = text;
  string16 prev_text = text;
  while (iter.Advance()) {
    size_t pos = iter.pos();
    if (pos >= text.length())
      break;
    // This can be a low surrogate codepoint, but u_isUWhiteSpace will
    // return false and inserting a new line after a surrogate pair
    // is perfectly ok.
    char16 line_end_char = text[pos - 1];
    if (u_isUWhiteSpace(line_end_char))
      current_text.replace(pos - 1, 1, 1, char16('\n'));
    else
      current_text.insert(pos, 1, char16('\n'));
    dangerous_download_label_->SetText(current_text);
    size = dangerous_download_label_->GetPreferredSize();

    if (min_width == -1)
      min_width = size.width();

    // If the width is growing again, it means we passed the optimal width spot.
    if (size.width() > min_width) {
      dangerous_download_label_->SetText(prev_text);
      break;
    } else {
      min_width = size.width();
    }

    // Restore the string.
    prev_text = current_text;
    current_text = text;
  }

  // If we have a line with no line breaking opportunity (which is very
  // unlikely), we won't cut it.
  if (min_width == -1)
    size = dangerous_download_label_->GetPreferredSize();

  dangerous_download_label_->SetBounds(0, 0, size.width(), size.height());
  dangerous_download_label_sized_ = true;
}

void DownloadItemView::Reenable() {
  disabled_while_opening_ = false;
  SetEnabled(true);  // Triggers a repaint.
}

bool DownloadItemView::InDropDownButtonXCoordinateRange(int x) {
  if (x > drop_down_x_left_ && x < drop_down_x_right_)
    return true;
  return false;
}

void DownloadItemView::UpdateAccessibleName() {
  string16 new_name;
  if (download_->GetSafetyState() == DownloadItem::DANGEROUS) {
    new_name = dangerous_download_label_->GetText();
  } else {
    new_name = status_text_ + char16(' ') +
        download_->GetFileNameToReportUser().LossyDisplayName();
  }

  // If the name has changed, notify assistive technology that the name
  // has changed so they can announce it immediately.
  if (new_name != accessible_name_) {
    accessible_name_ = new_name;
    if (GetWidget()) {
      GetWidget()->NotifyAccessibilityEvent(
          this, ui::AccessibilityTypes::EVENT_NAME_CHANGED, true);
    }
  }
}
