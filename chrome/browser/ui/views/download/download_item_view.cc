// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "unicode/uchar.h"

// TODO(paulg): These may need to be adjusted when download progress
//              animation is added, and also possibly to take into account
//              different screen resolutions.
static const int kTextWidth = 140;            // Pixels
static const int kDangerousTextWidth = 200;   // Pixels
static const int kHorizontalTextPadding = 2;  // Pixels
static const int kVerticalPadding = 3;        // Pixels
static const int kVerticalTextSpacer = 2;     // Pixels
static const int kVerticalTextPadding = 2;    // Pixels
static const int kTooltipMaxWidth = 800;      // Pixels

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

using content::DownloadItem;

DownloadItemView::DownloadItemView(DownloadItem* download,
    DownloadShelfView* parent,
    BaseDownloadItemModel* model)
  : warning_icon_(NULL),
    download_(download),
    parent_(parent),
    status_text_(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING)),
    body_state_(NORMAL),
    drop_down_state_(NORMAL),
    mode_(NORMAL_MODE),
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
    ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(download_);
  download_->AddObserver(this);
  set_context_menu_controller(this);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  BodyImageSet normal_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM)
  };
  normal_body_image_set_ = normal_body_image_set;

  DropDownImageSet normal_drop_down_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM)
  };
  normal_drop_down_image_set_ = normal_drop_down_image_set;

  BodyImageSet hot_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H)
  };
  hot_body_image_set_ = hot_body_image_set;

  DropDownImageSet hot_drop_down_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H)
  };
  hot_drop_down_image_set_ = hot_drop_down_image_set;

  BodyImageSet pushed_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P)
  };
  pushed_body_image_set_ = pushed_body_image_set;

  DropDownImageSet pushed_drop_down_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P)
  };
  pushed_drop_down_image_set_ = pushed_drop_down_image_set;

  BodyImageSet dangerous_mode_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_NO_DD),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_NO_DD),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_NO_DD)
  };
  dangerous_mode_body_image_set_ = dangerous_mode_body_image_set;

  malicious_mode_body_image_set_ = normal_body_image_set;

  LoadIcon();

  font_ = rb.GetFont(ui::ResourceBundle::BaseFont);
  box_height_ = std::max<int>(2 * kVerticalPadding + font_.GetHeight() +
                                  kVerticalTextPadding + font_.GetHeight(),
                              2 * kVerticalPadding +
                                  normal_body_image_set_.top_left->height() +
                                  normal_body_image_set_.bottom_left->height());

  if (download_util::kSmallProgressIconSize > box_height_)
    box_y_ = (download_util::kSmallProgressIconSize - box_height_) / 2;
  else
    box_y_ = kVerticalPadding;

  body_hover_animation_.reset(new ui::SlideAnimation(this));
  drop_hover_animation_.reset(new ui::SlideAnimation(this));

  UpdateDropDownButtonPosition();

  tooltip_text_ = model_->GetTooltipText(font_, kTooltipMaxWidth);

  if (model_->IsDangerous())
    ShowWarningDialog();

  UpdateAccessibleName();
  set_accessibility_focusable(true);

  // Set up our animation.
  StartDownloadProgress();
}

DownloadItemView::~DownloadItemView() {
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

  if (IsShowingWarningDialog() && !model_->IsDangerous()) {
    // We have been approved.
    ClearWarningDialog();
  } else if (!IsShowingWarningDialog() && model_->IsDangerous()) {
    ShowWarningDialog();
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
      default:
        NOTREACHED();
    }
    status_text_ = status_text;
  }

  string16 new_tip = model_->GetTooltipText(font_, kTooltipMaxWidth);
  if (new_tip != tooltip_text_) {
    tooltip_text_ = new_tip;
    TooltipTextChanged();
  }

  UpdateAccessibleName();

  // We use the parent's (DownloadShelfView's) SchedulePaint, since there
  // are spaces between each DownloadItemView that the parent is responsible
  // for painting.
  parent()->SchedulePaint();
}

void DownloadItemView::OnDownloadDestroyed(DownloadItem* download) {
  parent_->RemoveDownloadView(this);  // This will delete us!
}

void DownloadItemView::OnDownloadOpened(DownloadItem* download) {
  disabled_while_opening_ = true;
  SetEnabled(false);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DownloadItemView::Reenable,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDisabledOnOpenDuration));

  // Notify our parent.
  parent_->OpenedDownload(this);
}

// View overrides

// In dangerous mode we have to layout our buttons.
void DownloadItemView::Layout() {
  if (IsShowingWarningDialog()) {
    BodyImageSet* body_image_set =
        (mode_ == DANGEROUS_MODE) ? &dangerous_mode_body_image_set_ :
            &malicious_mode_body_image_set_;
    int x = kLeftPadding + body_image_set->top_left->width() +
        warning_icon_->width() + kLabelPadding;
    int y = (height() - dangerous_download_label_->height()) / 2;
    dangerous_download_label_->SetBounds(x, y,
                                         dangerous_download_label_->width(),
                                         dangerous_download_label_->height());
    gfx::Size button_size = GetButtonSize();
    x += dangerous_download_label_->width() + kLabelPadding;
    y = (height() - button_size.height()) / 2;
    if (save_button_) {
      save_button_->SetBounds(x, y, button_size.width(), button_size.height());
      x += button_size.width() + kButtonPadding;
    }
    discard_button_->SetBounds(x, y, button_size.width(), button_size.height());
    UpdateColorsFromTheme();
  }
}

gfx::Size DownloadItemView::GetPreferredSize() {
  int width, height;

  // First, we set the height to the height of two rows or text plus margins.
  height = 2 * kVerticalPadding + 2 * font_.GetHeight() + kVerticalTextPadding;
  // Then we increase the size if the progress icon doesn't fit.
  height = std::max<int>(height, download_util::kSmallProgressIconSize);

  if (IsShowingWarningDialog()) {
    BodyImageSet* body_image_set =
        (mode_ == DANGEROUS_MODE) ? &dangerous_mode_body_image_set_ :
            &malicious_mode_body_image_set_;
    width = kLeftPadding + body_image_set->top_left->width();
    width += warning_icon_->width() + kLabelPadding;
    width += dangerous_download_label_->width() + kLabelPadding;
    gfx::Size button_size = GetButtonSize();
    // Make sure the button fits.
    height = std::max<int>(height, 2 * kVerticalPadding + button_size.height());
    // Then we make sure the warning icon fits.
    height = std::max<int>(height, 2 * kVerticalPadding +
                                   warning_icon_->height());
    if (save_button_)
      width += button_size.width() + kButtonPadding;
    width += button_size.width();
    width += body_image_set->top_right->width();
    if (mode_ == MALICIOUS_MODE)
      width += normal_drop_down_image_set_.top->width();
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
bool DownloadItemView::OnMousePressed(const ui::MouseEvent& event) {
  HandlePressEvent(event, event.IsOnlyLeftMouseButton());
  return true;
}

// Handle drag (file copy) operations.
bool DownloadItemView::OnMouseDragged(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
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

void DownloadItemView::OnMouseReleased(const ui::MouseEvent& event) {
  HandleClickEvent(event, event.IsOnlyLeftMouseButton());
}

void DownloadItemView::OnMouseCaptureLost() {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    starting_drag_ = false;
  } else {
    SetState(NORMAL, NORMAL);
  }
}

void DownloadItemView::OnMouseMoved(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  bool on_body = !InDropDownButtonXCoordinateRange(event.x());
  SetState(on_body ? HOT : NORMAL, on_body ? NORMAL : HOT);
  if (on_body) {
    if (!IsShowingWarningDialog())
      body_hover_animation_->Show();
    drop_hover_animation_->Hide();
  } else {
    if (!IsShowingWarningDialog())
      body_hover_animation_->Hide();
    drop_hover_animation_->Show();
  }
}

void DownloadItemView::OnMouseExited(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  SetState(NORMAL, drop_down_pressed_ ? PUSHED : NORMAL);
  if (!IsShowingWarningDialog())
    body_hover_animation_->Hide();
  drop_hover_animation_->Hide();
}

bool DownloadItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Key press should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
    return true;

  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    OpenDownload();
    return true;
  }
  return false;
}

ui::EventResult DownloadItemView::OnGestureEvent(
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP_DOWN) {
    HandlePressEvent(event, true);
    return ui::ER_CONSUMED;
  }

  if (event.type() == ui::ET_GESTURE_TAP) {
    HandleClickEvent(event, true);
    return ui::ER_CONSUMED;
  }

  SetState(NORMAL, NORMAL);
  return views::View::OnGestureEvent(event);
}

bool DownloadItemView::GetTooltipText(const gfx::Point& p,
                                      string16* tooltip) const {
  if (IsShowingWarningDialog()) {
    tooltip->clear();
    return false;
  }

  tooltip->assign(tooltip_text_);

  return true;
}

void DownloadItemView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = accessible_name_;
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  if (model_->IsDangerous()) {
    state->state = ui::AccessibilityTypes::STATE_UNAVAILABLE;
  } else {
    state->state = ui::AccessibilityTypes::STATE_HASPOPUP;
  }
}

void DownloadItemView::OnThemeChanged() {
  UpdateColorsFromTheme();
}

void DownloadItemView::ShowContextMenuForView(View* source,
                                              const gfx::Point& point) {
  // |point| is in screen coordinates. So convert it to local coordinates first.
  gfx::Point local_point = point;
  ConvertPointFromScreen(this, &local_point);
  ShowContextMenuImpl(local_point, true);
}

void DownloadItemView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender == discard_button_) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                             base::Time::Now() - creation_time_);
    if (download_->IsPartialDownload())
      download_->Cancel(true);
    download_->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
    // WARNING: we are deleted at this point.  Don't access 'this'.
  } else if (save_button_ && sender == save_button_) {
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

// The DownloadItemView can be in three major modes (NORMAL_MODE, DANGEROUS_MODE
// and MALICIOUS_MODE).
//
// NORMAL_MODE: We are displaying an in-progress or completed download.
// .-------------------------------+-.
// | [icon] Filename               |v|
// | [    ] Status                 | |
// `-------------------------------+-'
//  |  |                            \_ Drop down button. Invokes menu. Responds
//  |  |                               to mouse. (NORMAL, HOT or PUSHED).
//  |   \_ Icon is overlaid on top of in-progress animation.
//   \_ Both the body and the drop down button respond to mouse hover and can be
//      pushed (NORMAL, HOT or PUSHED).
//
// DANGEROUS_MODE: The file could be potentially dangerous.
// .-------------------------------------------------------.
// | [ ! ] [This type of file can  ]  [ Keep ] [ Discard ] |
// | [   ] [destroy your computer..]  [      ] [         ] |
// `-------------------------------------------------------'
//  |  |    |                          |                 \_ No drop down button.
//  |  |    |                           \_ Buttons are views::TextButtons.
//  |  |     \_ Text is in a label (dangerous_download_label_)
//  |   \_ Warning icon.  No progress animation.
//   \_ Body is static.  Doesn't respond to mouse hover or press. (NORMAL only)
//
// MALICIOUS_MODE: The file is known malware.
// .---------------------------------------------+-.
// | [ - ] [This file is malicious.] [ Discard ] |v|
// | [   ] [                       ] [         ] | |-.
// `---------------------------------------------+-' |
//  |  |    |                         |            Drop down button. Responds to
//  |  |    |                         |            mouse.(NORMAL, HOT or PUSHED)
//  |  |    |                          \_ Button is a views::TextButton.
//  |  |     \_ Text is in a label (dangerous_download_label_)
//  |   \_ Warning icon.  No progress animation.
//   \_ Body is static.  Doesn't respond to mouse hover or press. (NORMAL only)
//
void DownloadItemView::OnPaint(gfx::Canvas* canvas) {
  BodyImageSet* body_image_set = NULL;
  switch (mode_) {
    case NORMAL_MODE:
      if (body_state_ == PUSHED)
        body_image_set = &pushed_body_image_set_;
      else                      // NORMAL or HOT
        body_image_set = &normal_body_image_set_;
      break;
    case DANGEROUS_MODE:
      body_image_set = &dangerous_mode_body_image_set_;
      break;
    case MALICIOUS_MODE:
      body_image_set = &malicious_mode_body_image_set_;
      break;
    default:
      NOTREACHED();
  }

  DropDownImageSet* drop_down_image_set = NULL;
  switch (mode_) {
    case NORMAL_MODE:
    case MALICIOUS_MODE:
      if (drop_down_state_ == PUSHED)
        drop_down_image_set = &pushed_drop_down_image_set_;
      else                        // NORMAL or HOT
        drop_down_image_set = &normal_drop_down_image_set_;
      break;
    case DANGEROUS_MODE:
      // We don't use a drop down button for mode_ == DANGEROUS_MODE.  So we let
      // drop_down_image_set == NULL.
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

  // Draw status before button image to effectively lighten text.  No status for
  // warning dialogs.
  if (!IsShowingWarningDialog()) {
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
  PaintImages(canvas,
              body_image_set->top_left, body_image_set->left,
              body_image_set->bottom_left,
              x, box_y_, box_height_, body_image_set->top_left->width());
  x += body_image_set->top_left->width();
  PaintImages(canvas,
              body_image_set->top, body_image_set->center,
              body_image_set->bottom,
              x, box_y_, box_height_, center_width);
  x += center_width;
  PaintImages(canvas,
              body_image_set->top_right, body_image_set->right,
              body_image_set->bottom_right,
              x, box_y_, box_height_, body_image_set->top_right->width());

  // Overlay our body hot state. Warning dialogs don't display body a hot state.
  if (!IsShowingWarningDialog() &&
      body_hover_animation_->GetCurrentValue() > 0) {
    canvas->SaveLayerAlpha(
        static_cast<int>(body_hover_animation_->GetCurrentValue() * 255));
    canvas->sk_canvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);

    int x = kLeftPadding;
    PaintImages(canvas,
                hot_body_image_set_.top_left, hot_body_image_set_.left,
                hot_body_image_set_.bottom_left,
                x, box_y_, box_height_, hot_body_image_set_.top_left->width());
    x += body_image_set->top_left->width();
    PaintImages(canvas,
                hot_body_image_set_.top, hot_body_image_set_.center,
                hot_body_image_set_.bottom,
                x, box_y_, box_height_, center_width);
    x += center_width;
    PaintImages(canvas,
                hot_body_image_set_.top_right, hot_body_image_set_.right,
                hot_body_image_set_.bottom_right,
                x, box_y_, box_height_,
                hot_body_image_set_.top_right->width());
    canvas->Restore();
  }

  x += body_image_set->top_right->width();

  // Paint the drop-down.
  if (drop_down_image_set) {
    PaintImages(canvas,
                drop_down_image_set->top, drop_down_image_set->center,
                drop_down_image_set->bottom,
                x, box_y_, box_height_, drop_down_image_set->top->width());

    // Overlay our drop-down hot state.
    if (drop_hover_animation_->GetCurrentValue() > 0) {
      canvas->SaveLayerAlpha(
          static_cast<int>(drop_hover_animation_->GetCurrentValue() * 255));
      canvas->sk_canvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);

      PaintImages(canvas,
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
  if (!IsShowingWarningDialog()) {
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
                          enabled() ? file_name_color
                                    : kFileNameDisabledColor,
                          mirrored_x, y, kTextWidth, font_.GetHeight());
  }

  // Load the icon.
  IconManager* im = g_browser_process->icon_manager();
  gfx::Image* image = im->LookupIcon(download_->GetUserVerifiedFilePath(),
                                     IconLoader::SMALL);
  const gfx::ImageSkia* icon = NULL;
  if (IsShowingWarningDialog())
    icon = warning_icon_;
  else if (image)
    icon = image->ToImageSkia();

  // We count on the fact that the icon manager will cache the icons and if one
  // is available, it will be cached here. We *don't* want to request the icon
  // to be loaded here, since this will also get called if the icon can't be
  // loaded, in which case LookupIcon will always be NULL. The loading will be
  // triggered only when we think the status might change.
  if (icon) {
    if (!IsShowingWarningDialog()) {
      if (download_->IsInProgress()) {
        download_util::PaintDownloadProgress(canvas, this, 0, 0,
                                             progress_angle_,
                                             model_->PercentComplete(),
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

    if (IsShowingWarningDialog()) {
      icon_x = kLeftPadding + body_image_set->top_left->width();
      icon_y = (height() - icon->height()) / 2;
    } else {
      icon_x = download_util::kSmallProgressIconOffset;
      icon_y = download_util::kSmallProgressIconOffset;
    }
    icon_x = GetMirroredXWithWidthInView(icon_x, icon->width());
    if (enabled()) {
      canvas->DrawImageInt(*icon, icon_x, icon_y);
    } else {
      // Use an alpha to make the image look disabled.
      SkPaint paint;
      paint.setAlpha(120);
      canvas->DrawImageInt(*icon, icon_x, icon_y, paint);
    }
  }
}

void DownloadItemView::OpenDownload() {
  DCHECK(!IsShowingWarningDialog());
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

void DownloadItemView::UpdateColorsFromTheme() {
  if (dangerous_download_label_ && GetThemeProvider()) {
    dangerous_download_label_->SetEnabledColor(
        GetThemeProvider()->GetColor(ThemeService::COLOR_BOOKMARK_TEXT));
  }
}

void DownloadItemView::ShowContextMenuImpl(const gfx::Point& p,
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
  // Post a task to release the button.  When we call the Run method on the menu
  // below, it runs an inner message loop that might cause us to be deleted.
  // Posting a task with a WeakPtr lets us safely handle the button release.
  MessageLoop::current()->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&DownloadItemView::ReleaseDropDown,
                 weak_ptr_factory_.GetWeakPtr()));
  views::View::ConvertPointToScreen(this, &point);

  if (!context_menu_.get()) {
    context_menu_.reset(
        new DownloadShelfContextMenuView(model_.get(),
                                         parent_->GetNavigator()));
  }
  context_menu_->Run(GetWidget()->GetTopLevelWidget(),
                     gfx::Rect(point, size));
  // We could be deleted now.
}

void DownloadItemView::HandlePressEvent(const ui::LocatedEvent& event,
                                        bool active_event) {
  // The event should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  // Stop any completion animation.
  if (complete_animation_.get() && complete_animation_->is_animating())
    complete_animation_->End();

  if (active_event) {
    if (InDropDownButtonXCoordinateRange(event.x())) {
      drop_down_pressed_ = true;
      SetState(NORMAL, PUSHED);
      // We are setting is_mouse_gesture to false when calling ShowContextMenu
      // so that the positioning of the context menu will be similar to a
      // keyboard invocation.  I.e. we want the menu to always be positioned
      // next to the drop down button instead of the next to the pointer.
      ShowContextMenuImpl(event.location(), false);
      // Once called, it is possible that *this was deleted (e.g.: due to
      // invoking the 'Discard' action.)
    } else if (!IsShowingWarningDialog()) {
      SetState(PUSHED, NORMAL);
    }
  }
}

void DownloadItemView::HandleClickEvent(const ui::LocatedEvent& event,
                                        bool active_event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  if (active_event &&
      !InDropDownButtonXCoordinateRange(event.x()) &&
      !IsShowingWarningDialog()) {
    OpenDownload();
  }

  SetState(NORMAL, NORMAL);
}

// Load an icon for the file type we're downloading, and animate any in progress
// download state.
void DownloadItemView::PaintImages(gfx::Canvas* canvas,
                                   const gfx::ImageSkia* top_image,
                                   const gfx::ImageSkia* center_image,
                                   const gfx::ImageSkia* bottom_image,
                                   int x, int y, int height, int width) {
  int middle_height = height - top_image->height() - bottom_image->height();
  // Draw the top.
  canvas->DrawImageInt(*top_image,
                       0, 0, top_image->width(), top_image->height(),
                       x, y, width, top_image->height(), false);
  y += top_image->height();
  // Draw the center.
  canvas->DrawImageInt(*center_image,
                       0, 0, center_image->width(), center_image->height(),
                       x, y, width, middle_height, false);
  y += middle_height;
  // Draw the bottom.
  canvas->DrawImageInt(*bottom_image,
                       0, 0, bottom_image->width(), bottom_image->height(),
                       x, y, width, bottom_image->height(), false);
}

void DownloadItemView::SetState(State body_state, State drop_down_state) {
  // If we are showing a warning dialog, we don't change body state.
  if (IsShowingWarningDialog()) {
    body_state = NORMAL;

    // Current body_state_ should always be NORMAL for warning dialogs.
    DCHECK(body_state_ == NORMAL);
    // We shouldn't be calling SetState if we are in DANGEROUS_MODE.
    DCHECK(mode_ != DANGEROUS_MODE);
  }
  // Avoid extra SchedulePaint()s if the state is going to be the same.
  if (body_state_ == body_state && drop_down_state_ == drop_down_state)
    return;

  body_state_ = body_state;
  drop_down_state_ = drop_down_state;
  SchedulePaint();
}

void DownloadItemView::ClearWarningDialog() {
  DCHECK(download_->GetSafetyState() == DownloadItem::DANGEROUS_BUT_VALIDATED &&
         (mode_ == DANGEROUS_MODE || mode_ == MALICIOUS_MODE));

  mode_ = NORMAL_MODE;
  body_state_ = NORMAL;
  drop_down_state_ = NORMAL;

  // Remove the views used by the warning dialog.
  if (save_button_) {
    RemoveChildView(save_button_);
    delete save_button_;
    save_button_ = NULL;
  }
  RemoveChildView(discard_button_);
  delete discard_button_;
  discard_button_ = NULL;
  RemoveChildView(dangerous_download_label_);
  delete dangerous_download_label_;
  dangerous_download_label_ = NULL;
  dangerous_download_label_sized_ = false;
  cached_button_size_.SetSize(0,0);

  // Set the accessible name back to the status and filename instead of the
  // download warning.
  UpdateAccessibleName();
  UpdateDropDownButtonPosition();

  // We need to load the icon now that the download_ has the real path.
  LoadIcon();

  // Force the shelf to layout again as our size has changed.
  parent_->Layout();
  parent_->SchedulePaint();

  TooltipTextChanged();
}

void DownloadItemView::ShowWarningDialog() {
  DCHECK(mode_ != DANGEROUS_MODE && mode_ != MALICIOUS_MODE);
  mode_ = ((model_->IsMalicious()) ? MALICIOUS_MODE : DANGEROUS_MODE);

  body_state_ = NORMAL;
  drop_down_state_ = NORMAL;
  if (mode_ == DANGEROUS_MODE) {
    save_button_ = new views::NativeTextButton(
        this, model_->GetWarningConfirmButtonText());
    save_button_->set_ignore_minimum_size(true);
    AddChildView(save_button_);
  }
  discard_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_DISCARD_DOWNLOAD));
  discard_button_->set_ignore_minimum_size(true);
  AddChildView(discard_button_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // The dangerous download label text and icon are different under
  // different cases.
  if (mode_ == MALICIOUS_MODE) {
    warning_icon_ = rb.GetImageSkiaNamed(IDR_SAFEBROWSING_WARNING);
  } else {
    // The download file has dangerous file type (e.g.: an executable).
    warning_icon_ = rb.GetImageSkiaNamed(IDR_WARNING);
  }
  string16 dangerous_label = model_->GetWarningText(font_, kTextWidth);
  dangerous_download_label_ = new views::Label(dangerous_label);
  dangerous_download_label_->SetMultiLine(true);
  dangerous_download_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  dangerous_download_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(dangerous_download_label_);
  SizeLabelToMinWidth();
  UpdateDropDownButtonPosition();
  TooltipTextChanged();
}

gfx::Size DownloadItemView::GetButtonSize() {
  DCHECK(discard_button_ && (mode_ == MALICIOUS_MODE || save_button_));
  gfx::Size size;

  // We cache the size when successfully retrieved, not for performance reasons
  // but because if this DownloadItemView is being animated while the tab is
  // not showing, the native buttons are not parented and their preferred size
  // is 0, messing-up the layout.
  if (cached_button_size_.width() != 0)
    return cached_button_size_;

  if (save_button_)
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

  string16 label_text = dangerous_download_label_->text();
  TrimWhitespace(label_text, TRIM_ALL, &label_text);
  DCHECK_EQ(string16::npos, label_text.find('\n'));

  // Make the label big so that GetPreferredSize() is not constrained by the
  // current width.
  dangerous_download_label_->SetBounds(0, 0, 1000, 1000);

  // Use a const string from here. BreakIterator requies that text.data() not
  // change during its lifetime.
  const string16 original_text(label_text);
  // Using BREAK_WORD can work in most cases, but it can also break
  // lines where it should not. Using BREAK_LINE is safer although
  // slower for Chinese/Japanese. This is not perf-critical at all, though.
  base::i18n::BreakIterator iter(original_text,
                                 base::i18n::BreakIterator::BREAK_LINE);
  bool status = iter.Init();
  DCHECK(status);

  string16 prev_text = original_text;
  gfx::Size size = dangerous_download_label_->GetPreferredSize();
  int min_width = size.width();

  // Go through the string and try each line break (starting with no line break)
  // searching for the optimal line break position.  Stop if we find one that
  // yields one that is less than kDangerousTextWidth wide.  This is to prevent
  // a short string (e.g.: "This file is malicious") from being broken up
  // unnecessarily.
  while (iter.Advance() && min_width > kDangerousTextWidth) {
    size_t pos = iter.pos();
    if (pos >= original_text.length())
      break;
    string16 current_text = original_text;
    // This can be a low surrogate codepoint, but u_isUWhiteSpace will
    // return false and inserting a new line after a surrogate pair
    // is perfectly ok.
    char16 line_end_char = current_text[pos - 1];
    if (u_isUWhiteSpace(line_end_char))
      current_text.replace(pos - 1, 1, 1, char16('\n'));
    else
      current_text.insert(pos, 1, char16('\n'));
    dangerous_download_label_->SetText(current_text);
    size = dangerous_download_label_->GetPreferredSize();

    // If the width is growing again, it means we passed the optimal width spot.
    if (size.width() > min_width) {
      dangerous_download_label_->SetText(prev_text);
      break;
    } else {
      min_width = size.width();
    }
    prev_text = current_text;
  }

  dangerous_download_label_->SetBounds(0, 0, size.width(), size.height());
  dangerous_download_label_sized_ = true;
}

void DownloadItemView::Reenable() {
  disabled_while_opening_ = false;
  SetEnabled(true);  // Triggers a repaint.
}

void DownloadItemView::ReleaseDropDown() {
  drop_down_pressed_ = false;
  SetState(NORMAL, NORMAL);
}

bool DownloadItemView::InDropDownButtonXCoordinateRange(int x) {
  if (x > drop_down_x_left_ && x < drop_down_x_right_)
    return true;
  return false;
}

void DownloadItemView::UpdateAccessibleName() {
  string16 new_name;
  if (IsShowingWarningDialog()) {
    new_name = dangerous_download_label_->text();
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

void DownloadItemView::UpdateDropDownButtonPosition() {
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
}
