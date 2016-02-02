// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_item_view_md.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/bar_control_button.h"
#include "chrome/browser/ui/views/download/download_feedback_dialog_view.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_danger_type.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using content::DownloadItem;
using extensions::ExperienceSamplingEvent;

namespace {

// All values in dp.
const int kTextWidth = 140;
const int kDangerousTextWidth = 200;

// The normal height of the item which may be exceeded if text is large.
const int kDefaultHeight = 48;

// The vertical distance between the item's visual upper bound (as delineated by
// the separator on the right) and the edge of the shelf.
const int kTopBottomPadding = 6;

// The minimum vertical padding above and below contents of the download item.
// This is only used when the text size is large.
const int kMinimumVerticalPadding = 2 + kTopBottomPadding;

// Vertical padding between filename and status text.
const int kVerticalTextPadding = 1;

const int kTooltipMaxWidth = 800;

// Padding before the icon and at end of the item.
const int kStartPadding = 12;
const int kEndPadding = 19;

// Horizontal padding between progress indicator and filename/status text.
const int kProgressTextPadding = 8;

// The space between the Save and Discard buttons when prompting for a dangerous
// download.
const int kButtonPadding = 5;

// The space on the left and right side of the dangerous download label.
const int kLabelPadding = 8;

// Height/width of the warning icon, also in dp.
const int kWarningIconSize = 24;

// How long the 'download complete' animation should last for.
const int kCompleteAnimationDurationMs = 2500;

// How long the 'download interrupted' animation should last for.
const int kInterruptedAnimationDurationMs = 2500;

// How long we keep the item disabled after the user clicked it to open the
// downloaded item.
const int kDisabledOnOpenDuration = 3000;

// The separator is drawn as a border. It's one dp wide.
class SeparatorBorder : public views::Border {
 public:
  explicit SeparatorBorder(SkColor color) : color_(color) {}
  ~SeparatorBorder() override {}

  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    int end_x = base::i18n::IsRTL() ? 0 : view.width() - 1;
    canvas->DrawLine(gfx::Point(end_x, kTopBottomPadding),
                     gfx::Point(end_x, view.height() - kTopBottomPadding),
                     color_);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(0, 0, 0, 1); }

  gfx::Size GetMinimumSize() const override {
    return gfx::Size(1, 2 * kTopBottomPadding + 1);
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SeparatorBorder);
};

}  // namespace

DownloadItemViewMd::DownloadItemViewMd(DownloadItem* download_item,
                                       DownloadShelfView* parent)
    : shelf_(parent),
      status_text_(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING)),
      dropdown_state_(NORMAL),
      mode_(NORMAL_MODE),
      dragging_(false),
      starting_drag_(false),
      model_(download_item),
      save_button_(nullptr),
      discard_button_(nullptr),
      dropdown_button_(new BarControlButton(this)),
      dangerous_download_label_(nullptr),
      dangerous_download_label_sized_(false),
      disabled_while_opening_(false),
      creation_time_(base::Time::Now()),
      time_download_warning_shown_(base::Time()),
      weak_ptr_factory_(this) {
  DCHECK(download());
  DCHECK(ui::MaterialDesignController::IsModeMaterial());
  download()->AddObserver(this);
  set_context_menu_controller(this);

  AddChildView(dropdown_button_);

  LoadIcon();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  font_list_ =
      rb.GetFontList(ui::ResourceBundle::BaseFont).DeriveWithSizeDelta(1);
  status_font_list_ =
      rb.GetFontList(ui::ResourceBundle::BaseFont).DeriveWithSizeDelta(-2);

  body_hover_animation_.reset(new gfx::SlideAnimation(this));
  drop_hover_animation_.reset(new gfx::SlideAnimation(this));

  SetAccessibilityFocusable(true);

  OnDownloadUpdated(download());

  SetDropdownState(NORMAL);
  UpdateColorsFromTheme();
}

DownloadItemViewMd::~DownloadItemViewMd() {
  StopDownloadProgress();
  download()->RemoveObserver(this);

  // ExperienceSampling: If the user took no action to remove the warning
  // before it disappeared, then the user effectively dismissed the download
  // without keeping it.
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kIgnore);
}

// Progress animation handlers.

void DownloadItemViewMd::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_start_time_ = base::TimeTicks::Now();
  progress_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
                                       DownloadShelf::kProgressRateMs),
                        base::Bind(&DownloadItemViewMd::ProgressTimerFired,
                                   base::Unretained(this)));
}

void DownloadItemViewMd::StopDownloadProgress() {
  if (!progress_timer_.IsRunning())
    return;
  previous_progress_elapsed_ += base::TimeTicks::Now() - progress_start_time_;
  progress_start_time_ = base::TimeTicks();
  progress_timer_.Stop();
}

// static
SkColor DownloadItemViewMd::GetTextColorForThemeProvider(
    const ui::ThemeProvider* theme) {
  return theme ? theme->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)
               : gfx::kPlaceholderColor;
}

void DownloadItemViewMd::OnExtractIconComplete(gfx::Image* icon_bitmap) {
  if (icon_bitmap)
    shelf_->SchedulePaint();
}

// DownloadObserver interface.

// Update the progress graphic on the icon and our text status label
// to reflect our current bytes downloaded, time remaining.
void DownloadItemViewMd::OnDownloadUpdated(DownloadItem* download_item) {
  DCHECK_EQ(download(), download_item);

  if (!model_.ShouldShowInShelf()) {
    shelf_->RemoveDownloadView(this);  // This will delete us!
    return;
  }

  if (IsShowingWarningDialog() != model_.IsDangerous()) {
    ToggleWarningDialog();
  } else {
    switch (download()->GetState()) {
      case DownloadItem::IN_PROGRESS:
        download()->IsPaused() ? StopDownloadProgress()
                               : StartDownloadProgress();
        LoadIconIfItemPathChanged();
        break;
      case DownloadItem::INTERRUPTED:
        StopDownloadProgress();
        complete_animation_.reset(new gfx::SlideAnimation(this));
        complete_animation_->SetSlideDuration(kInterruptedAnimationDurationMs);
        complete_animation_->SetTweenType(gfx::Tween::LINEAR);
        complete_animation_->Show();
        LoadIcon();
        break;
      case DownloadItem::COMPLETE:
        if (model_.ShouldRemoveFromShelfWhenComplete()) {
          shelf_->RemoveDownloadView(this);  // This will delete us!
          return;
        }
        StopDownloadProgress();
        complete_animation_.reset(new gfx::SlideAnimation(this));
        complete_animation_->SetSlideDuration(kCompleteAnimationDurationMs);
        complete_animation_->SetTweenType(gfx::Tween::LINEAR);
        complete_animation_->Show();
        LoadIcon();
        break;
      case DownloadItem::CANCELLED:
        StopDownloadProgress();
        if (complete_animation_)
          complete_animation_->Stop();
        LoadIcon();
        break;
      default:
        NOTREACHED();
    }
    status_text_ = model_.GetStatusText();
    SchedulePaint();
  }

  base::string16 new_tip = model_.GetTooltipText(font_list_, kTooltipMaxWidth);
  if (new_tip != tooltip_text_) {
    tooltip_text_ = new_tip;
    TooltipTextChanged();
  }

  UpdateAccessibleName();
}

void DownloadItemViewMd::OnDownloadDestroyed(DownloadItem* download) {
  shelf_->RemoveDownloadView(this);  // This will delete us!
}

void DownloadItemViewMd::OnDownloadOpened(DownloadItem* download) {
  disabled_while_opening_ = true;
  SetEnabled(false);
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DownloadItemViewMd::Reenable, weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDisabledOnOpenDuration));

  // Notify our parent.
  shelf_->OpenedDownload();
}

// View overrides

// In dangerous mode we have to layout our buttons.
void DownloadItemViewMd::Layout() {
  UpdateColorsFromTheme();

  if (IsShowingWarningDialog()) {
    int x = kStartPadding + kWarningIconSize + kStartPadding;
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
  } else {
    dropdown_button_->SizeToPreferredSize();
    dropdown_button_->SetPosition(
        gfx::Point(width() - dropdown_button_->width(),
                   (height() - dropdown_button_->height()) / 2));
  }
}

gfx::Size DownloadItemViewMd::GetPreferredSize() const {
  int width, height;

  // First, we set the height to the height of two rows or text plus margins.
  height = std::max(kDefaultHeight,
                    2 * kMinimumVerticalPadding + font_list_.GetBaseline() +
                        kVerticalTextPadding + status_font_list_.GetHeight());

  if (IsShowingWarningDialog()) {
    width = kStartPadding + kWarningIconSize + kLabelPadding +
            dangerous_download_label_->width() + kLabelPadding;
    gfx::Size button_size = GetButtonSize();
    // Make sure the button fits.
    height = std::max<int>(height,
                           2 * kMinimumVerticalPadding + button_size.height());
    // Then we make sure the warning icon fits.
    height = std::max<int>(
        height, 2 * kMinimumVerticalPadding + kWarningIconSize);
    if (save_button_)
      width += button_size.width() + kButtonPadding;
    width += button_size.width() + kEndPadding;
  } else {
    width = kStartPadding + DownloadShelf::kProgressIndicatorSize +
            kProgressTextPadding + kTextWidth + kEndPadding;
  }
  return gfx::Size(width, height);
}

// Handle a mouse click and open the context menu if the mouse is
// over the drop-down region.
bool DownloadItemViewMd::OnMousePressed(const ui::MouseEvent& event) {
  HandlePressEvent(event, event.IsOnlyLeftMouseButton());
  return true;
}

// Handle drag (file copy) operations.
bool DownloadItemViewMd::OnMouseDragged(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
    return true;

  if (!starting_drag_) {
    starting_drag_ = true;
    drag_start_point_ = event.location();
  }
  if (dragging_) {
    if (download()->GetState() == DownloadItem::COMPLETE) {
      IconManager* im = g_browser_process->icon_manager();
      gfx::Image* icon = im->LookupIconFromFilepath(
          download()->GetTargetFilePath(), IconLoader::SMALL);
      views::Widget* widget = GetWidget();
      DragDownloadItem(download(), icon,
                       widget ? widget->GetNativeView() : NULL);
    }
  } else if (ExceededDragThreshold(event.location() - drag_start_point_)) {
    dragging_ = true;
  }
  return true;
}

void DownloadItemViewMd::OnMouseReleased(const ui::MouseEvent& event) {
  HandleClickEvent(event, event.IsOnlyLeftMouseButton());
}

void DownloadItemViewMd::OnMouseCaptureLost() {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    starting_drag_ = false;
  }
}

bool DownloadItemViewMd::OnKeyPressed(const ui::KeyEvent& event) {
  // Key press should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
    return true;

  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    // OpenDownload may delete this, so don't add any code after this line.
    OpenDownload();
    return true;
  }
  return false;
}

bool DownloadItemViewMd::GetTooltipText(const gfx::Point& p,
                                        base::string16* tooltip) const {
  if (IsShowingWarningDialog()) {
    tooltip->clear();
    return false;
  }

  tooltip->assign(tooltip_text_);

  return true;
}

void DownloadItemViewMd::GetAccessibleState(ui::AXViewState* state) {
  state->name = accessible_name_;
  state->role = ui::AX_ROLE_BUTTON;
  if (model_.IsDangerous())
    state->AddStateFlag(ui::AX_STATE_DISABLED);
  else
    state->AddStateFlag(ui::AX_STATE_HASPOPUP);
}

void DownloadItemViewMd::OnThemeChanged() {
  UpdateColorsFromTheme();
}

void DownloadItemViewMd::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    HandlePressEvent(*event, true);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP) {
    HandleClickEvent(*event, true);
    event->SetHandled();
    return;
  }

  views::View::OnGestureEvent(event);
}

void DownloadItemViewMd::ShowContextMenuForView(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ShowContextMenuImpl(gfx::Rect(point, gfx::Size()), source_type);
}

void DownloadItemViewMd::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == dropdown_button_) {
    // TODO(estade): this is copied from ToolbarActionView but should be shared
    // one way or another.
    ui::MenuSourceType type = ui::MENU_SOURCE_NONE;
    if (event.IsMouseEvent())
      type = ui::MENU_SOURCE_MOUSE;
    else if (event.IsKeyEvent())
      type = ui::MENU_SOURCE_KEYBOARD;
    else if (event.IsGestureEvent())
      type = ui::MENU_SOURCE_TOUCH;
    SetDropdownState(PUSHED);
    ShowContextMenuImpl(dropdown_button_->GetBoundsInScreen(), type);
    return;
  }

  base::TimeDelta warning_duration;
  if (!time_download_warning_shown_.is_null())
    warning_duration = base::Time::Now() - time_download_warning_shown_;

  if (save_button_ && sender == save_button_) {
    // The user has confirmed a dangerous download.  We'd record how quickly the
    // user did this to detect whether we're being clickjacked.
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download", warning_duration);
    // ExperienceSampling: User chose to proceed with a dangerous download.
    if (sampling_event_.get()) {
      sampling_event_->CreateUserDecisionEvent(
          ExperienceSamplingEvent::kProceed);
      sampling_event_.reset(NULL);
    }
    // This will change the state and notify us.
    download()->ValidateDangerousDownload();
    return;
  }

  // WARNING: all end states after this point delete |this|.
  DCHECK_EQ(discard_button_, sender);
  if (model_.IsMalicious()) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.dismiss_download", warning_duration);
    // ExperienceSampling: User chose to dismiss the dangerous download.
    if (sampling_event_.get()) {
      sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
      sampling_event_.reset(NULL);
    }
    shelf_->RemoveDownloadView(this);
    return;
  }
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download", warning_duration);
  if (model_.ShouldAllowDownloadFeedback() &&
      !shelf_->browser()->profile()->IsOffTheRecord()) {
    if (!shelf_->browser()->profile()->GetPrefs()->HasPrefPath(
            prefs::kSafeBrowsingExtendedReportingEnabled)) {
      // Show dialog, because the dialog hasn't been shown before.
      DownloadFeedbackDialogView::Show(
          shelf_->get_parent()->GetNativeWindow(), shelf_->browser()->profile(),
          shelf_->GetNavigator(),
          base::Bind(
              &DownloadItemViewMd::PossiblySubmitDownloadToFeedbackService,
              weak_ptr_factory_.GetWeakPtr()));
    } else {
      PossiblySubmitDownloadToFeedbackService(
          shelf_->browser()->profile()->GetPrefs()->GetBoolean(
              prefs::kSafeBrowsingExtendedReportingEnabled));
    }
    return;
  }
  download()->Remove();
}

void DownloadItemViewMd::AnimationProgressed(const gfx::Animation* animation) {
  // We don't care if what animation (body button/drop button/complete),
  // is calling back, as they all have to go through the same paint call.
  SchedulePaint();
}

void DownloadItemViewMd::OnPaint(gfx::Canvas* canvas) {
  DrawStatusText(canvas);
  DrawFilename(canvas);
  DrawIcon(canvas);
  OnPaintBorder(canvas);

  if (HasFocus())
    canvas->DrawFocusRect(GetLocalBounds());
}

int DownloadItemViewMd::GetYForFilenameText() const {
  int text_height = font_list_.GetBaseline();
  if (!status_text_.empty())
    text_height += kVerticalTextPadding + status_font_list_.GetBaseline();
  return (height() - text_height) / 2;
}

void DownloadItemViewMd::DrawStatusText(gfx::Canvas* canvas) {
  if (status_text_.empty() || IsShowingWarningDialog())
    return;

  int mirrored_x = GetMirroredXWithWidthInView(
      kStartPadding + DownloadShelf::kProgressIndicatorSize +
          kProgressTextPadding,
      kTextWidth);
  int y =
      GetYForFilenameText() + font_list_.GetBaseline() + kVerticalTextPadding;
  canvas->DrawStringRect(
      status_text_, status_font_list_, GetDimmedTextColor(),
      gfx::Rect(mirrored_x, y, kTextWidth, status_font_list_.GetHeight()));
}

void DownloadItemViewMd::DrawFilename(gfx::Canvas* canvas) {
  if (IsShowingWarningDialog())
    return;

  // Print the text, left aligned and always print the file extension.
  // Last value of x was the end of the right image, just before the button.
  // Note that in dangerous mode we use a label (as the text is multi-line).
  base::string16 filename;
  if (!disabled_while_opening_) {
    filename = gfx::ElideFilename(download()->GetFileNameToReportUser(),
                                  font_list_, kTextWidth);
  } else {
    // First, Calculate the download status opening string width.
    base::string16 status_string = l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_OPENING, base::string16());
    int status_string_width = gfx::GetStringWidth(status_string, font_list_);
    // Then, elide the file name.
    base::string16 filename_string =
        gfx::ElideFilename(download()->GetFileNameToReportUser(), font_list_,
                           kTextWidth - status_string_width);
    // Last, concat the whole string.
    filename = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING,
                                          filename_string);
  }

  int mirrored_x = GetMirroredXWithWidthInView(
      kStartPadding + DownloadShelf::kProgressIndicatorSize +
          kProgressTextPadding,
      kTextWidth);
  canvas->DrawStringRect(filename, font_list_,
                         enabled() ? GetTextColor() : GetDimmedTextColor(),
                         gfx::Rect(mirrored_x, GetYForFilenameText(),
                                   kTextWidth, font_list_.GetHeight()));
}

void DownloadItemViewMd::DrawIcon(gfx::Canvas* canvas) {
  if (IsShowingWarningDialog()) {
    int icon_x = base::i18n::IsRTL()
                     ? width() - kWarningIconSize - kStartPadding
                     : kStartPadding;
    int icon_y = (height() - kWarningIconSize) / 2;
    canvas->DrawImageInt(GetWarningIcon(), icon_x, icon_y);
    return;
  }

  // Paint download progress.
  DownloadItem::DownloadState state = download()->GetState();
  canvas->Save();
  int progress_x =
      base::i18n::IsRTL()
          ? width() - kStartPadding - DownloadShelf::kProgressIndicatorSize
          : kStartPadding;
  int progress_y = (height() - DownloadShelf::kProgressIndicatorSize) / 2;
  canvas->Translate(gfx::Vector2d(progress_x, progress_y));

  if (state == DownloadItem::IN_PROGRESS) {
    base::TimeDelta progress_time = previous_progress_elapsed_;
    if (!download()->IsPaused())
      progress_time += base::TimeTicks::Now() - progress_start_time_;
    DownloadShelf::PaintDownloadProgress(
        canvas, *GetThemeProvider(), progress_time, model_.PercentComplete());
  } else if (complete_animation_.get() && complete_animation_->is_animating()) {
    if (state == DownloadItem::INTERRUPTED) {
      DownloadShelf::PaintDownloadInterrupted(
          canvas, *GetThemeProvider(), complete_animation_->GetCurrentValue());
    } else {
      DCHECK_EQ(DownloadItem::COMPLETE, state);
      DownloadShelf::PaintDownloadComplete(
          canvas, *GetThemeProvider(), complete_animation_->GetCurrentValue());
    }
  }
  canvas->Restore();

  // Fetch the already-loaded icon.
  IconManager* im = g_browser_process->icon_manager();
  gfx::Image* icon = im->LookupIconFromFilepath(download()->GetTargetFilePath(),
                                                IconLoader::SMALL);
  if (!icon)
    return;

  // Draw the icon image.
  int icon_x = progress_x + DownloadShelf::kFiletypeIconOffset;
  int icon_y = progress_y + DownloadShelf::kFiletypeIconOffset;
  SkPaint paint;
  // Use an alpha to make the image look disabled.
  if (!enabled())
    paint.setAlpha(120);
  canvas->DrawImageInt(*icon->ToImageSkia(), icon_x, icon_y, paint);
}

void DownloadItemViewMd::OnFocus() {
  View::OnFocus();
  // We render differently when focused.
  SchedulePaint();
}

void DownloadItemViewMd::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

void DownloadItemViewMd::OpenDownload() {
  DCHECK(!IsShowingWarningDialog());
  // We're interested in how long it takes users to open downloads.  If they
  // open downloads super quickly, we should be concerned about clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - creation_time_);

  UpdateAccessibleName();

  // Calling download()->OpenDownload may delete this, so this must be
  // the last thing we do.
  download()->OpenDownload();
}

bool DownloadItemViewMd::SubmitDownloadToFeedbackService() {
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service)
    return false;
  safe_browsing::DownloadProtectionService* download_protection_service =
      sb_service->download_protection_service();
  if (!download_protection_service)
    return false;
  download_protection_service->feedback_service()->BeginFeedbackForDownload(
      download());
  // WARNING: we are deleted at this point.  Don't access 'this'.
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

void DownloadItemViewMd::PossiblySubmitDownloadToFeedbackService(bool enabled) {
  if (!enabled || !SubmitDownloadToFeedbackService())
    download()->Remove();
  // WARNING: 'this' is deleted at this point. Don't access 'this'.
}

void DownloadItemViewMd::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  last_download_item_path_ = download()->GetTargetFilePath();
  im->LoadIcon(last_download_item_path_, IconLoader::SMALL,
               base::Bind(&DownloadItemViewMd::OnExtractIconComplete,
                          base::Unretained(this)),
               &cancelable_task_tracker_);
}

void DownloadItemViewMd::LoadIconIfItemPathChanged() {
  base::FilePath current_download_path = download()->GetTargetFilePath();
  if (last_download_item_path_ == current_download_path)
    return;

  LoadIcon();
}

void DownloadItemViewMd::UpdateColorsFromTheme() {
  if (!GetThemeProvider())
    return;

  if (dangerous_download_label_)
    dangerous_download_label_->SetEnabledColor(GetTextColor());
  SetBorder(make_scoped_ptr(new SeparatorBorder(
      GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR))));
}

void DownloadItemViewMd::ShowContextMenuImpl(const gfx::Rect& rect,
                                             ui::MenuSourceType source_type) {
  // Similar hack as in MenuButton.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to NULL.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())
      ->SetMouseHandler(NULL);

  // Post a task to release the button.  When we call the Run method on the menu
  // below, it runs an inner message loop that might cause us to be deleted.
  // Posting a task with a WeakPtr lets us safely handle the button release.
  base::MessageLoop::current()->task_runner()->PostNonNestableTask(
      FROM_HERE, base::Bind(&DownloadItemViewMd::ReleaseDropdown,
                            weak_ptr_factory_.GetWeakPtr()));

  if (!context_menu_.get())
    context_menu_.reset(new DownloadShelfContextMenuView(download()));
  context_menu_->Run(GetWidget()->GetTopLevelWidget(), rect, source_type);
  // We could be deleted now.
}

void DownloadItemViewMd::HandlePressEvent(const ui::LocatedEvent& event,
                                          bool active_event) {
  // The event should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  // Stop any completion animation.
  if (complete_animation_.get() && complete_animation_->is_animating())
    complete_animation_->End();
}

void DownloadItemViewMd::HandleClickEvent(const ui::LocatedEvent& event,
                                          bool active_event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  if (!active_event || IsShowingWarningDialog())
    return;

  // OpenDownload may delete this, so don't add any code after this line.
  OpenDownload();
}

void DownloadItemViewMd::SetDropdownState(State new_state) {
  // Avoid extra SchedulePaint()s if the state is going to be the same and
  // |dropdown_button_| has already been initialized.
  if (dropdown_state_ == new_state &&
      !dropdown_button_->GetImage(views::CustomButton::STATE_NORMAL).isNull())
    return;

  dropdown_button_->SetIcon(
      new_state == PUSHED ? gfx::VectorIconId::FIND_NEXT
                          : gfx::VectorIconId::FIND_PREV,
      base::Bind(&DownloadItemViewMd::GetTextColor, base::Unretained(this)));
  dropdown_button_->OnThemeChanged();
  dropdown_state_ = new_state;
  SchedulePaint();
}

void DownloadItemViewMd::ToggleWarningDialog() {
  if (model_.IsDangerous())
    ShowWarningDialog();
  else
    ClearWarningDialog();

  // We need to load the icon now that the download has the real path.
  LoadIcon();

  // Force the shelf to layout again as our size has changed.
  shelf_->Layout();
  shelf_->SchedulePaint();
}

void DownloadItemViewMd::ClearWarningDialog() {
  DCHECK(download()->GetDangerType() ==
         content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
  DCHECK(mode_ == DANGEROUS_MODE || mode_ == MALICIOUS_MODE);

  mode_ = NORMAL_MODE;
  dropdown_state_ = NORMAL;

  // ExperienceSampling: User proceeded through the warning.
  if (sampling_event_.get()) {
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
    sampling_event_.reset(NULL);
  }
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
  cached_button_size_.SetSize(0, 0);

  // We need to load the icon now that the download has the real path.
  LoadIcon();

  dropdown_button_->SetVisible(true);
}

void DownloadItemViewMd::ShowWarningDialog() {
  DCHECK(mode_ != DANGEROUS_MODE && mode_ != MALICIOUS_MODE);
  time_download_warning_shown_ = base::Time::Now();
  content::DownloadDangerType danger_type = download()->GetDangerType();
  RecordDangerousDownloadWarningShown(danger_type);
#if defined(FULL_SAFE_BROWSING)
  if (model_.ShouldAllowDownloadFeedback()) {
    safe_browsing::DownloadFeedbackService::RecordEligibleDownloadShown(
        danger_type);
  }
#endif
  mode_ = model_.MightBeMalicious() ? MALICIOUS_MODE : DANGEROUS_MODE;

  // ExperienceSampling: Dangerous or malicious download warning is being shown
  // to the user, so we start a new SamplingEvent and track it.
  std::string event_name = model_.MightBeMalicious()
                               ? ExperienceSamplingEvent::kMaliciousDownload
                               : ExperienceSamplingEvent::kDangerousDownload;
  sampling_event_.reset(new ExperienceSamplingEvent(
      event_name, download()->GetURL(), download()->GetReferrerUrl(),
      download()->GetBrowserContext()));

  dropdown_state_ = NORMAL;
  if (mode_ == DANGEROUS_MODE) {
    save_button_ =
        new views::LabelButton(this, model_.GetWarningConfirmButtonText());
    save_button_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(save_button_);
  }
  int discard_button_message =
      model_.IsMalicious() ? IDS_DISMISS_DOWNLOAD : IDS_DISCARD_DOWNLOAD;
  discard_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(discard_button_message));
  discard_button_->SetStyle(views::Button::STYLE_BUTTON);
  AddChildView(discard_button_);

  base::string16 dangerous_label =
      model_.GetWarningText(font_list_, kTextWidth);
  dangerous_download_label_ = new views::Label(dangerous_label);
  dangerous_download_label_->SetMultiLine(true);
  dangerous_download_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  dangerous_download_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(dangerous_download_label_);
  SizeLabelToMinWidth();

  dropdown_button_->SetVisible(false);
}

gfx::ImageSkia DownloadItemViewMd::GetWarningIcon() {
  switch (download()->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return gfx::CreateVectorIcon(gfx::VectorIconId::REMOVE_CIRCLE,
                                   kWarningIconSize,
                                   gfx::kGoogleRed700);

    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case content::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
      break;

    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return gfx::CreateVectorIcon(gfx::VectorIconId::WARNING,
                                   kWarningIconSize,
                                   gfx::kGoogleYellow700);
  }
  return gfx::ImageSkia();
}

gfx::Size DownloadItemViewMd::GetButtonSize() const {
  DCHECK(discard_button_ && (mode_ == MALICIOUS_MODE || save_button_));
  gfx::Size size;

  // We cache the size when successfully retrieved, not for performance reasons
  // but because if this DownloadItemViewMd is being animated while the tab is
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
void DownloadItemViewMd::SizeLabelToMinWidth() {
  if (dangerous_download_label_sized_)
    return;

  base::string16 label_text = dangerous_download_label_->text();
  base::TrimWhitespace(label_text, base::TRIM_ALL, &label_text);
  DCHECK_EQ(base::string16::npos, label_text.find('\n'));

  // Make the label big so that GetPreferredSize() is not constrained by the
  // current width.
  dangerous_download_label_->SetBounds(0, 0, 1000, 1000);

  // Use a const string from here. BreakIterator requies that text.data() not
  // change during its lifetime.
  const base::string16 original_text(label_text);
  // Using BREAK_WORD can work in most cases, but it can also break
  // lines where it should not. Using BREAK_LINE is safer although
  // slower for Chinese/Japanese. This is not perf-critical at all, though.
  base::i18n::BreakIterator iter(original_text,
                                 base::i18n::BreakIterator::BREAK_LINE);
  bool status = iter.Init();
  DCHECK(status);

  base::string16 prev_text = original_text;
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
    base::string16 current_text = original_text;
    // This can be a low surrogate codepoint, but u_isUWhiteSpace will
    // return false and inserting a new line after a surrogate pair
    // is perfectly ok.
    base::char16 line_end_char = current_text[pos - 1];
    if (u_isUWhiteSpace(line_end_char))
      current_text.replace(pos - 1, 1, 1, base::char16('\n'));
    else
      current_text.insert(pos, 1, base::char16('\n'));
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

void DownloadItemViewMd::Reenable() {
  disabled_while_opening_ = false;
  SetEnabled(true);  // Triggers a repaint.
}

void DownloadItemViewMd::ReleaseDropdown() {
  SetDropdownState(NORMAL);
}

void DownloadItemViewMd::UpdateAccessibleName() {
  base::string16 new_name;
  if (IsShowingWarningDialog()) {
    new_name = dangerous_download_label_->text();
  } else {
    new_name = status_text_ + base::char16(' ') +
               download()->GetFileNameToReportUser().LossyDisplayName();
  }

  // If the name has changed, notify assistive technology that the name
  // has changed so they can announce it immediately.
  if (new_name != accessible_name_) {
    accessible_name_ = new_name;
    NotifyAccessibilityEvent(ui::AX_EVENT_TEXT_CHANGED, true);
  }
}

void DownloadItemViewMd::AnimateStateTransition(
    State from,
    State to,
    gfx::SlideAnimation* animation) {
  if (from == NORMAL && to == HOT) {
    animation->Show();
  } else if (from == HOT && to == NORMAL) {
    animation->Hide();
  } else if (from != to) {
    animation->Reset((to == HOT) ? 1.0 : 0.0);
  }
}

void DownloadItemViewMd::ProgressTimerFired() {
  // Only repaint for the indeterminate size case. Otherwise, we'll repaint only
  // when there's an update notified via OnDownloadUpdated().
  if (model_.PercentComplete() < 0)
    SchedulePaint();
}

SkColor DownloadItemViewMd::GetTextColor() {
  return GetTextColorForThemeProvider(GetThemeProvider());
}

SkColor DownloadItemViewMd::GetDimmedTextColor() {
  return SkColorSetA(GetTextColor(), 0xC7);
}
