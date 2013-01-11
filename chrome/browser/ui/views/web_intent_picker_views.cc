// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ipc/ipc_message.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

using content::WebContents;
using views::GridLayout;

namespace {

// The color used to dim disabled elements.
const SkColor kHalfOpacityWhite = SkColorSetARGB(128, 255, 255, 255);

// The color used to display an enabled label.
const SkColor kEnabledLabelColor = SkColorSetRGB(51, 51, 51);

// The color used to display an enabled link.
const SkColor kEnabledLinkColor = SkColorSetRGB(17, 85, 204);

// The color used to display a disabled link.
const SkColor kDisabledLinkColor = SkColorSetRGB(128, 128, 128);

// The time between successive throbber frames in milliseconds.
const int kThrobberFrameTimeMs = 50;

// Width of IntentView action button in pixels
const int kButtonWidth = 130;

// Minimum number of action buttons - fill up with suggestions as needed.
const int kMinRowCount = 4;

// Maximum number of action buttons - do not add suggestions to reach.
const int kMaxRowCount = 8;

// The vertical padding around the UI elements in the waiting view.
const int kWaitingViewVerticalPadding = 40;

// Enables or disables all child views of |view|.
void EnableChildViews(views::View* view, bool enabled) {
  for (int i = 0; i < view->child_count(); ++i) {
    views::View* child = view->child_at(i);
    child->SetEnabled(enabled);
  }
}

// Create a "close" button.
views::ImageButton* CreateCloseButton(views::ButtonListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::ImageButton* close_button = new views::ImageButton(listener);
  close_button->SetImage(views::CustomButton::STATE_NORMAL,
                         rb.GetImageSkiaNamed(IDR_CLOSE_DIALOG));
  close_button->SetImage(views::CustomButton::STATE_HOVERED,
                         rb.GetImageSkiaNamed(IDR_CLOSE_DIALOG_H));
  close_button->SetImage(views::CustomButton::STATE_PRESSED,
                         rb.GetImageSkiaNamed(IDR_CLOSE_DIALOG_P));
  return close_button;
}

// Creates a label.
views::Label* CreateLabel() {
  views::Label* label = new views::Label();
  label->SetEnabledColor(kEnabledLabelColor);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

// Creates a title-style label.
views::Label* CreateTitleLabel() {
  views::Label* label = CreateLabel();
  label->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::MediumFont));
  const int kLabelBuiltinTopPadding = 5;
  label->set_border(views::Border::CreateEmptyBorder(
      WebIntentPicker::kContentAreaBorder -
          chrome_style::kCloseButtonPadding -
          kLabelBuiltinTopPadding,
      0, 0, 0));
  return label;
}

// Creates a link.
views::Link* CreateLink() {
  views::Link* link = new views::Link();
  link->SetEnabledColor(kEnabledLinkColor);
  link->SetDisabledColor(kDisabledLinkColor);
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return link;
}

// Creates a header for the inline disposition dialog.
views::View* CreateInlineDispositionHeader(
    views::ImageView* app_icon,
    views::Label* app_title,
    views::Link* use_another_service_link) {
  views::View* header = new views::View();
  views::GridLayout* grid_layout = new views::GridLayout(header);
  const int kIconBuiltinTopPadding = 6;
  grid_layout->SetInsets(
      WebIntentPicker::kContentAreaBorder -
          chrome_style::kCloseButtonPadding -
          kIconBuiltinTopPadding,
      0, 0, 0);
  header->SetLayoutManager(grid_layout);
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(0);
  header_cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // App icon.
  header_cs->AddPaddingColumn(0, WebIntentPicker::kIconTextPadding);
  header_cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0);  // App title.
  header_cs->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  header_cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Use another app link.
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(app_icon);
  grid_layout->AddView(app_title);
  grid_layout->AddView(use_another_service_link);
  header->Layout();
  return header;
}

// Checks whether the inline disposition dialog should show the link for using
// another service.
bool IsUseAnotherServiceVisible(WebIntentPickerModel* model) {
  DCHECK(model);
  return model->show_use_another_service() &&
      (model->GetInstalledServiceCount() > 1 ||
       model->GetSuggestedExtensionCount());
}


// StarsView -------------------------------------------------------------------

// A view that displays 5 stars: empty, full or half full, given a rating in
// the range [0,5].
class StarsView : public views::View {
 public:
  explicit StarsView(double rating);
  virtual ~StarsView();

 private:
  // The star rating to display, in the range [0,5].
  double rating_;

  DISALLOW_COPY_AND_ASSIGN(StarsView);
};

StarsView::StarsView(double rating)
    : rating_(rating) {
  const int kSpacing = 1;  // Spacing between stars in pixels.

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, kSpacing));

  for (int i = 0; i < 5; ++i) {
    views::ImageView* image = new views::ImageView();
    image->SetImage(rb.GetImageSkiaNamed(
        WebIntentPicker::GetNthStarImageIdFromCWSRating(rating, i)));
    AddChildView(image);
  }

  // TODO(binji): Add tooltip with text rating
  // "Average Rating: X.XX stars (YYYYY)"
  // Y = "1: Hated it, 2: Disliked it, 3: It was okay, 4: Liked it,
  //      5: Loved it"
  // Choose Y based on rounded X.
}

StarsView::~StarsView() {
}


// ThrobberNativeTextButton ----------------------------------------------------

// A native text button that can display a throbber in place of its icon. Much
// of the logic of this class is copied from ui/views/controls/throbber.h.
class ThrobberNativeTextButton : public views::NativeTextButton {
 public:
  ThrobberNativeTextButton(views::ButtonListener* listener,
                           const string16& text);
  virtual ~ThrobberNativeTextButton();

  // Start or stop the throbber.
  void StartThrobber();
  void StopThrobber();

  // Set the throbber bitmap to use. IDR_THROBBER is used by default.
  void SetFrames(const gfx::ImageSkia* frames);

  // Provide a preferred size, accomodating buttons wider than their text.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Set the width desired for this button.
  void set_preferred_width(int width) { preferred_width_ = width; }

 protected:
  virtual const gfx::ImageSkia& GetImageToPaint() const OVERRIDE;

 private:
  // The timer callback to schedule painting this view.
  void Run();

  // Image that contains the throbber frames.
  const gfx::ImageSkia* frames_;

  // The currently displayed frame, given to GetImageToPaint.
  mutable gfx::ImageSkia this_frame_;

  // How long one frame is displayed.
  base::TimeDelta frame_time_;

  // Used to schedule Run calls.
  base::RepeatingTimer<ThrobberNativeTextButton> timer_;

  // How many frames we have.
  int frame_count_;

  // Time when StartThrobber was called.
  base::TimeTicks start_time_;

  // Whether the throbber is shown an animating.
  bool running_;

  // The width this button should assume.
  int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberNativeTextButton);
};

ThrobberNativeTextButton::ThrobberNativeTextButton(
    views::ButtonListener* listener, const string16& text)
    : NativeTextButton(listener, text),
      frame_time_(base::TimeDelta::FromMilliseconds(kThrobberFrameTimeMs)),
      frame_count_(0),
      running_(false),
      preferred_width_(0) {
  SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_THROBBER).ToImageSkia());
}

ThrobberNativeTextButton::~ThrobberNativeTextButton() {
  StopThrobber();
}

void ThrobberNativeTextButton::StartThrobber() {
  if (running_)
    return;

  start_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, frame_time_, this, &ThrobberNativeTextButton::Run);
  running_ = true;

  SchedulePaint();
}

void ThrobberNativeTextButton::StopThrobber() {
  if (!running_)
    return;

  timer_.Stop();
  running_ = false;
}

void ThrobberNativeTextButton::SetFrames(const gfx::ImageSkia* frames) {
  frames_ = frames;
  DCHECK(frames_->width() > 0 && frames_->height() > 0);
  DCHECK(frames_->width() % frames_->height() == 0);
  frame_count_ = frames_->width() / frames_->height();
  PreferredSizeChanged();
}

gfx::Size ThrobberNativeTextButton::GetPreferredSize() {
  gfx::Size size = NativeTextButton::GetPreferredSize();
  if (preferred_width_)
    size.set_width(preferred_width_);
  return size;
}

const gfx::ImageSkia& ThrobberNativeTextButton::GetImageToPaint() const {
  if (!running_)
    return NativeTextButton::GetImageToPaint();

  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  const int current_frame =
      static_cast<int>(elapsed_time / frame_time_) % frame_count_;
  const int image_size = frames_->height();
  const int image_offset = current_frame * image_size;

  gfx::Rect subset_rect(image_offset, 0, image_size, image_size);
  this_frame_ = gfx::ImageSkiaOperations::ExtractSubset(*frames_, subset_rect);
  return this_frame_;
}

void ThrobberNativeTextButton::Run() {
  DCHECK(running_);

  SchedulePaint();
}


// SpinnerProgressIndicator ----------------------------------------------------
class SpinnerProgressIndicator : public views::View {
 public:
  SpinnerProgressIndicator();
  virtual ~SpinnerProgressIndicator();

  void SetPercentDone(int percent);
  void SetIndeterminate(bool indetereminate);

  // Overridden from views::View.
  virtual void Paint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  void UpdateTimer();
  int GetProgressAngle();

  static const int kTimerIntervalMs = 1000 / 30;
  static const int kSpinRateDegreesPerSecond = 270;

  int percent_done_;
  int indeterminate_;

  base::TimeTicks start_time_;
  base::RepeatingTimer<SpinnerProgressIndicator> timer_;

  DISALLOW_COPY_AND_ASSIGN(SpinnerProgressIndicator);
};

SpinnerProgressIndicator::SpinnerProgressIndicator()
    : percent_done_(0),
      indeterminate_(true) {}

SpinnerProgressIndicator::~SpinnerProgressIndicator() {
}

void SpinnerProgressIndicator::SetPercentDone(int percent) {
  percent_done_ = percent;
  SchedulePaint();
  UpdateTimer();
}

void SpinnerProgressIndicator::SetIndeterminate(bool indetereminate) {
  indeterminate_ = indetereminate;
  SchedulePaint();
  UpdateTimer();
}

void SpinnerProgressIndicator::Paint(gfx::Canvas* canvas) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* fg = rb.GetImageSkiaNamed(IDR_WEB_INTENT_PROGRESS_FOREGROUND);
  gfx::ImageSkia* bg = rb.GetImageSkiaNamed(IDR_WEB_INTENT_PROGRESS_BACKGROUND);
  download_util::PaintCustomDownloadProgress(
      canvas,
      *bg,
      *fg,
      fg->width(),
      bounds(),
      GetProgressAngle(),
      indeterminate_ ? -1 : percent_done_);
}

gfx::Size SpinnerProgressIndicator::GetPreferredSize() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* fg = rb.GetImageSkiaNamed(IDR_WEB_INTENT_PROGRESS_FOREGROUND);
  return fg->size();
}

void SpinnerProgressIndicator::UpdateTimer() {
  if (!parent() || !indeterminate_) {
    timer_.Stop();
    return;
  }

  if (!timer_.IsRunning()) {
    start_time_ = base::TimeTicks::Now();
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimerIntervalMs),
        this, &SpinnerProgressIndicator::SchedulePaint);
  }
}

int SpinnerProgressIndicator::GetProgressAngle() {
  if (!indeterminate_)
    return download_util::kStartAngleDegrees;
  base::TimeDelta delta = base::TimeTicks::Now() - start_time_;
  int angle = delta.InSecondsF() * kSpinRateDegreesPerSecond;
  return angle % 360;
}


// WaitingView ----------------------------------------------------------
class WaitingView : public views::View {
 public:
  WaitingView(views::ButtonListener* listener, bool use_close_button);
  virtual ~WaitingView();

 private:
  DISALLOW_COPY_AND_ASSIGN(WaitingView);
};

WaitingView::WaitingView(views::ButtonListener* listener,
                         bool use_close_button) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(WebIntentPicker::kWindowMinWidth, 0));
  const int kMessageBuiltinBottomPadding = 3;
  layout->SetInsets(chrome_style::kCloseButtonPadding,
                    0,
                    kWaitingViewVerticalPadding - kMessageBuiltinBottomPadding,
                    0);
  SetLayoutManager(layout);

  enum GridLayoutColumnSets {
    HEADER_ROW,
    CONTENT_ROW,
  };
  views::ColumnSet* header_cs = layout->AddColumnSet(HEADER_ROW);
  header_cs->AddPaddingColumn(1, 1);
  header_cs->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                       GridLayout::USE_PREF, 0, 0);
  header_cs->AddPaddingColumn(
      0, chrome_style::kCloseButtonPadding);

  views::ColumnSet* content_cs = layout->AddColumnSet(CONTENT_ROW);
  content_cs->AddPaddingColumn(0, views::kPanelHorizIndentation);
  content_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  content_cs->AddPaddingColumn(0, views::kPanelHorizIndentation);

  // Close button
  layout->StartRow(0, HEADER_ROW);
  views::ImageButton* close_button = CreateCloseButton(listener);
  layout->AddView(close_button);
  close_button->SetVisible(use_close_button);

  // Throbber
  layout->AddPaddingRow(0,
                        kWaitingViewVerticalPadding -
                            chrome_style::kCloseButtonPadding -
                            close_button->GetPreferredSize().height());
  layout->StartRow(0, CONTENT_ROW);
  SpinnerProgressIndicator* throbber = new SpinnerProgressIndicator();
  layout->AddView(throbber);

  // Message
  const int kMessageBuiltinTopPadding = 5;
  layout->AddPaddingRow(0,
                        chrome_style::kRowPadding -
                            kMessageBuiltinTopPadding);
  layout->StartRow(0, CONTENT_ROW);
  views::Label* label = CreateLabel();
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label->SetText(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_WAIT_FOR_CWS));
  layout->AddView(label);

  // Start the throbber.
  throbber->SetIndeterminate(true);
}

WaitingView::~WaitingView() {
}


// IntentRowView --------------------------------------------------

// A view for each row in the IntentsView. It displays information
// for both installed and suggested intent handlers.
class IntentRowView : public views::View,
                      public views::ButtonListener,
                      public views::LinkListener {
 public:
  enum ActionType {
    ACTION_UNKNOWN,
    ACTION_INSTALL,
    ACTION_INVOKE
  };

  class Delegate {
   public:
    // Called when the user clicks the "Add to Chrome" button.
    virtual void OnExtensionInstallClicked(const std::string& extension_id) = 0;

    // Called when the user clicks the extension title link.
    virtual void OnExtensionLinkClicked(
        const std::string& extension_id,
        WindowOpenDisposition disposition) = 0;

    // Called when the action button is clicked. |type| indicates the requested
    // kind of action, and |tag| identifies the service or extension to
    // operate on.
    virtual void OnActionButtonClicked(ActionType type, size_t tag) = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~IntentRowView();

  // Creates a new view for |service| or |extension| depending on which
  // value is not NULL.
  static IntentRowView* CreateHandlerRow(
      const WebIntentPickerModel::InstalledService* service,
      const WebIntentPickerModel::SuggestedExtension* extension,
      int tag,
      IntentRowView::Delegate* delegate,
      int preferred_width);

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // LinkListener implementation.
  void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Start an animating throbber for this row, and hide the star rating and the
  // install button.
  void StartThrobber();

  void MarkBusy(const std::string& extension_id);

  // Stop the throbber for this row, and show the star rating and the install
  // button.
  void StopThrobber();

 protected:
  virtual void OnEnabledChanged() OVERRIDE;

  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

 private:
  IntentRowView(ActionType type, size_t tag);

  // Gets the proper message string associated with |type_|.
  string16 GetActionButtonMessage();

  // Identifier for the suggested extension displayed in this row.
  std::string extension_id_;

  // A delegate to respond to button presses and clicked links.
  Delegate* delegate_;

  // The icon of the extension.
  views::ImageView* icon_;

  // The title of the extension, which links to the CWS detailed description of
  // this extension.
  views::View* title_link_;

  // The star rating of this extension.
  StarsView* stars_;

  // A button to install the extension.
  ThrobberNativeTextButton* install_button_;

  // The type of action that is invoked from the row's button.
  ActionType type_;

  // A tag identifying the data associated with this row. For both installed
  // and suggested services, this is an index into the respective collections
  // on the WebIntentPickerModel.
  size_t tag_;

  DISALLOW_COPY_AND_ASSIGN(IntentRowView);
};

IntentRowView::IntentRowView(ActionType type, size_t tag)
    : delegate_(NULL),
      icon_(NULL),
      title_link_(NULL),
      stars_(NULL),
      install_button_(NULL),
      type_(type),
      tag_(tag) {}

IntentRowView* IntentRowView::CreateHandlerRow(
    const WebIntentPickerModel::InstalledService* service,
    const WebIntentPickerModel::SuggestedExtension* extension,
    int tag,
    IntentRowView::Delegate* delegate,
    int preferred_width) {

  // one or the other must be set...exclusively
  DCHECK((service != NULL || extension != NULL) &&
         (service == NULL || extension == NULL));


  const string16& title = (service != NULL)
      ? service->title : extension->title;

  // TODO(groby): Once links are properly sized (see SuggestionRowViewLayout
  // refactor notes), can simply SetElideBehavior(views::Label::ELIDE_AT_END).
  // Note: Verify that views links do not treat empty space at the end as
  // part of the link, if this change happens.
  string16 elided_title = ui::ElideText(title, gfx::Font(),
                                        WebIntentPicker::kTitleLinkMaxWidth,
                                        ui::ELIDE_AT_END);

  const gfx::ImageSkia* icon = NULL;
  StarsView* stars = NULL;
  views::Label* label = NULL;
  IntentRowView* view;
  if (service != NULL) {
    view = new IntentRowView(ACTION_INVOKE, tag);
    icon = service->favicon.ToImageSkia();
    label = CreateLabel();
    label->SetText(elided_title);
  } else {
    view = new IntentRowView(ACTION_INSTALL, tag);
    view->extension_id_ = extension->id;
    icon = extension->icon.ToImageSkia();
    views::Link* link = CreateLink();
    link->SetText(elided_title);
    link->set_listener(view);
    label = link;
    stars = new StarsView(extension->average_rating);
  }

  view->delegate_ = delegate;

  views::GridLayout* grid_layout = new views::GridLayout(view);
  view->SetLayoutManager(grid_layout);

  views::ColumnSet* columns = grid_layout->AddColumnSet(0);
  columns->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                     GridLayout::USE_PREF, 0, 0); // Icon.
  columns->AddPaddingColumn(0, WebIntentPicker::kIconTextPadding);
  columns->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                     GridLayout::FIXED, WebIntentPicker::kTitleLinkMaxWidth, 0);
  const int kStarRatingHorizontalSpacing = 20;
  columns->AddPaddingColumn(0, kStarRatingHorizontalSpacing);
  if (stars != NULL) {
    columns->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0); // Star rating.
    columns->AddPaddingColumn(0, kStarRatingHorizontalSpacing);
  }
  columns->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                     GridLayout::FIXED, preferred_width, 0); // Button.

  grid_layout->StartRow(0, 0);

  view->icon_ = new views::ImageView();
  view->icon_->SetImage(icon);
  grid_layout->AddView(view->icon_);

  view->title_link_ = label;
  grid_layout->AddView(view->title_link_);

  if (stars != NULL) {
    view->stars_ = stars;
    grid_layout->AddView(view->stars_);
  }

  view->install_button_ = new ThrobberNativeTextButton(
      view, view->GetActionButtonMessage());
  view->install_button_->set_preferred_width(preferred_width);
  grid_layout->AddView(view->install_button_);

  return view;
}

IntentRowView::~IntentRowView() {}

void IntentRowView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (type_ == ACTION_INSTALL)
    delegate_->OnExtensionInstallClicked(extension_id_);
  else
    delegate_->OnActionButtonClicked(type_, tag_);
}

void IntentRowView::LinkClicked(views::Link* source,
                                int event_flags) {
  delegate_->OnExtensionLinkClicked(extension_id_,
      chrome::DispositionFromEventFlags(event_flags));
}

void IntentRowView::MarkBusy(const std::string& extension_id) {
  SetEnabled(false);
  if (extension_id == extension_id_)
    StartThrobber();
}

void IntentRowView::StartThrobber() {
  install_button_->StartThrobber();
  install_button_->SetText(string16());
}

void IntentRowView::StopThrobber() {
  install_button_->StopThrobber();
  install_button_->SetText(GetActionButtonMessage());
}

void IntentRowView::OnEnabledChanged() {
  title_link_->SetEnabled(enabled());
  if (stars_)
    stars_->SetEnabled(enabled());
  install_button_->SetEnabled(enabled());
  View::OnEnabledChanged();
  Layout();
}

void IntentRowView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  if (!enabled())
    canvas->FillRect(GetLocalBounds(), kHalfOpacityWhite);
}

string16 IntentRowView::GetActionButtonMessage() {
  int message_id = 0;

  if (type_ == ACTION_INVOKE)
    message_id = IDS_INTENT_PICKER_SELECT_INTENT;
  else  if (type_ == ACTION_INSTALL)
    message_id = IDS_INTENT_PICKER_INSTALL_EXTENSION;
  else
    NOTREACHED();

  return l10n_util::GetStringUTF16(message_id);
}


// IntentsView -----------------------------------------------------

// A view that contains both installed services and suggested extensions
// from the Chrome Web Store that provide an intent service matching the
// action/type pair.
class IntentsView : public views::View {
 public:
  IntentsView(const WebIntentPickerModel* model,
              IntentRowView::Delegate* delegate);

  virtual ~IntentsView();

  // Update the view to the new model data.
  void Update();

  // Show the install throbber for the row containing |extension_id|. This
  // function also hides hides and disables other buttons and links.
  void StartThrobber(const std::string& extension_id);

  // Hide the install throbber. This function re-enables all buttons and links.
  void StopThrobber();

  // Adjusts a given width to account for language-specific strings.
  int AdjustWidth(int old_width);

 protected:
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  const WebIntentPickerModel* model_;
  IntentRowView::Delegate* delegate_;

  // Width for the action button, adjusted to be wide enough for all possible
  // strings.
  int button_width_;

  DISALLOW_COPY_AND_ASSIGN(IntentsView);
};

IntentsView::IntentsView(
    const WebIntentPickerModel* model,
    IntentRowView::Delegate* delegate)
    : model_(model),
      delegate_(delegate),
      button_width_(0){
  Update();
}

IntentsView::~IntentsView() {
}

void IntentsView::Update() {
  RemoveAllChildViews(true);

  ThrobberNativeTextButton size_helper(
      NULL, l10n_util::GetStringUTF16(IDS_INTENT_PICKER_INSTALL_EXTENSION));
  size_helper.SetText(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_SELECT_INTENT));
  button_width_ = std::max(
      kButtonWidth, size_helper.GetPreferredSize().width());

  const int kAppRowVerticalSpacing = 10;
  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0, 0, kAppRowVerticalSpacing);
  SetLayoutManager(layout);

  int available_rows = kMaxRowCount;

  for (size_t i = 0;
      available_rows > 0 && i < model_->GetInstalledServiceCount();
      ++i, --available_rows) {
    const WebIntentPickerModel::InstalledService& service =
      model_->GetInstalledServiceAt(i);
    AddChildView(IntentRowView::CreateHandlerRow(&service, NULL, i,
                                                 delegate_, button_width_));
  }

  // Only fill up with suggestions if we filled less than kMinRowCount rows.
  available_rows -= (kMaxRowCount - kMinRowCount);

  for (size_t i = 0;
      available_rows > 0 && i < model_->GetSuggestedExtensionCount();
      ++i, --available_rows) {
    const WebIntentPickerModel::SuggestedExtension& extension =
        model_->GetSuggestedExtensionAt(i);
    AddChildView(IntentRowView::CreateHandlerRow(NULL, &extension, i,
                                                 delegate_, button_width_));
  }
}

void IntentsView::StartThrobber(const std::string& extension_id) {
  for (int i = 0; i < child_count(); ++i)
    static_cast<IntentRowView*>(child_at(i))->MarkBusy(extension_id);
}

void IntentsView::StopThrobber() {
  for (int i = 0; i < child_count(); ++i) {
    IntentRowView* row =
        static_cast<IntentRowView*>(child_at(i));
    row->SetEnabled(true);
    row->StopThrobber();
  }
}

int IntentsView::AdjustWidth(int old_width) {
  return old_width - kButtonWidth + button_width_;
}

void IntentsView::OnEnabledChanged() {
  EnableChildViews(this, enabled());
  View::OnEnabledChanged();
}

}  // namespace


// WebIntentPickerViews --------------------------------------------------------

// Views implementation of WebIntentPicker.
class WebIntentPickerViews : public views::ButtonListener,
                             public views::WidgetDelegate,
                             public views::LinkListener,
                             public WebIntentPicker,
                             public WebIntentPickerModelObserver,
                             public IntentRowView::Delegate {
 public:
  WebIntentPickerViews(WebContents* web_contents,
                       WebIntentPickerDelegate* delegate,
                       WebIntentPickerModel* model);
  virtual ~WebIntentPickerViews();

  // views::ButtonListener implementation.
  // This method is called when the user cancels the picker dialog.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::WidgetDelegate implementation.
  virtual void WindowClosing() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;
  virtual void SetActionString(const string16& action) OVERRIDE;
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE;
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) OVERRIDE;
  virtual void OnPendingAsyncCompleted() OVERRIDE;
  virtual void InvalidateDelegate() OVERRIDE;
  virtual void OnInlineDispositionWebContentsLoaded(
      content::WebContents* web_contents) OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnExtensionIconChanged(WebIntentPickerModel* model,
                                      const std::string& extension_id) OVERRIDE;
  virtual void OnInlineDisposition(const string16& title,
                                   const GURL& url) OVERRIDE;

  // SuggestedExtensionsRowView::Delegate implementation.
  virtual void OnExtensionInstallClicked(
      const std::string& extension_id) OVERRIDE;
  virtual void OnExtensionLinkClicked(
      const std::string& extension_id,
      WindowOpenDisposition disposition) OVERRIDE;
  virtual void OnActionButtonClicked(
      IntentRowView::ActionType type,
      size_t tag) OVERRIDE;

 private:
  enum WebIntentPickerViewsState {
    INITIAL,
    WAITING,
    NO_SERVICES,
    LIST_SERVICES,
    INLINE_SERVICE,
  } state_;

  // Update picker contents to reflect the current state of the model.
  void UpdateContents();

  // Shows a spinner and notifies the user that we are waiting for information
  // from the Chrome Web Store.
  void ShowWaitingForSuggestions();

  // Updates the dialog with the list of available services, suggestions,
  // and a nice link to CWS to find more suggestions. This is the "Main"
  // view of the picker.
  void ShowAvailableServices();

  // Informs the user that there are no services available to handle
  // the intent, and that there are no suggestions from the Chrome Web Store.
  void ShowNoServicesMessage();

  // Restore the contents of the picker to the initial contents.
  void ResetContents();

  // Resize the constrained window to the size of its contents.
  void SizeToContents();

  // Clear the contents of the picker.
  void ClearContents();

  // Returns the service selection question text used in the title
  // of the picker.
  const string16 GetActionTitle();

  // Refresh the icon for the inline disposition service that is being
  // displayed.
  void RefreshInlineServiceIcon();

  // Refresh the extensions control in the picker.
  void RefreshExtensions();

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // A weak pointer to the picker model.
  WebIntentPickerModel* model_;

  // A weak pointer to the action string label.
  // Created locally, owned by Views.
  views::Label* action_label_;

  // A weak pointer to the intents view.
  // Created locally, owned by Views view hierarchy.
  IntentsView* extensions_;

  // Delegate for inline disposition tab contents.
  scoped_ptr<WebIntentInlineDispositionDelegate> inline_disposition_delegate_;

  // A weak pointer to the WebContents this picker is in.
  WebContents* web_contents_;

  // A weak pointer to the WebView that hosts the WebContents being displayed.
  // Created locally, owned by Views.
  views::WebView* webview_;

  // A weak pointer to the view that contains all other views in the picker.
  // Created locally, owned by Views.
  views::View* contents_;

  // A weak pointer to the constrained window.
  // Created locally, owned by Views.
  ConstrainedWindowViews* window_;

  // A weak pointer to the more suggestions link.
  // Created locally, owned by Views.
  views::Link* more_suggestions_link_;

  // The icon for the inline disposition service.
  views::ImageView* inline_service_icon_;

  // A weak pointer to the choose another service link.
  // Created locally, owned by Views.
  views::Link* choose_another_service_link_;

  // Weak pointer to "Waiting for CWS" display. Owned by parent view.
  WaitingView* waiting_view_;

  // The text for the current action.
  string16 action_text_;

  // Ownership of the WebContents we are displaying in the inline disposition.
  scoped_ptr<WebContents> inline_web_contents_;

  // Indicate if dialog should display its own close button.
  // TODO(groby): Only relevant until new WebContentsModalDialog is implemented,
  // from then on always true.
  bool use_close_button_;

  // Signals if the picker can be closed. False during extension install.
  bool can_close_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerViews);
};

// static
WebIntentPicker* WebIntentPicker::Create(content::WebContents* web_contents,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerViews(web_contents, delegate, model);
}

WebIntentPickerViews::WebIntentPickerViews(WebContents* web_contents,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
    : state_(INITIAL),
      delegate_(delegate),
      model_(model),
      action_label_(NULL),
      extensions_(NULL),
      web_contents_(web_contents),
      webview_(new views::WebView(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      window_(NULL),
      more_suggestions_link_(NULL),
      inline_service_icon_(NULL),
      choose_another_service_link_(NULL),
      waiting_view_(NULL),
      can_close_(true) {
  use_close_button_ = false;

  model_->set_observer(this);
  contents_ = new views::View();
  contents_->set_background(views::Background::CreateSolidBackground(
      chrome_style::GetBackgroundColor()));

  // Show the dialog.
  window_ = new ConstrainedWindowViews(web_contents, this);
  if (model_->IsInlineDisposition())
    OnInlineDisposition(string16(), model_->inline_disposition_url());
  else
    UpdateContents();
}

WebIntentPickerViews::~WebIntentPickerViews() {
  model_->set_observer(NULL);
}

void WebIntentPickerViews::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  DCHECK(delegate_);
  delegate_->OnUserCancelledPickerDialog();
}

void WebIntentPickerViews::WindowClosing() {
  if (delegate_)
    delegate_->OnClosing();
}

void WebIntentPickerViews::DeleteDelegate() {
  delete this;
}

views::Widget* WebIntentPickerViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* WebIntentPickerViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* WebIntentPickerViews::GetContentsView() {
  return contents_;
}

void WebIntentPickerViews::LinkClicked(views::Link* source, int event_flags) {
  DCHECK(delegate_);
  if (source == more_suggestions_link_) {
    delegate_->OnSuggestionsLinkClicked(
        chrome::DispositionFromEventFlags(event_flags));
  } else if (source == choose_another_service_link_) {
    // Signal cancellation of inline disposition.
    delegate_->OnChooseAnotherService();
    ResetContents();
  } else {
    NOTREACHED();
  }
}

void WebIntentPickerViews::Close() {
  window_->CloseWebContentsModalDialog();
}

void WebIntentPickerViews::SetActionString(const string16& action) {
  action_text_ = action;
  if (action_label_) {
    action_label_->SetText(GetActionTitle());
    contents_->Layout();
    SizeToContents();
  }
}

void WebIntentPickerViews::OnExtensionInstallSuccess(const std::string& id) {
  can_close_ = true;
}

void WebIntentPickerViews::OnExtensionInstallFailure(const std::string& id) {
  extensions_->StopThrobber();
  extensions_->SetEnabled(true);
  more_suggestions_link_->SetEnabled(true);
  can_close_ = true;
  contents_->Layout();
  SizeToContents();

  // TODO(binji): What to display to user on failure?
}

void WebIntentPickerViews::OnInlineDispositionAutoResize(
    const gfx::Size& size) {
  webview_->SetPreferredSize(size);
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnPendingAsyncCompleted() {
  UpdateContents();
}

void WebIntentPickerViews::InvalidateDelegate() {
  delegate_ = NULL;
}

void WebIntentPickerViews::ShowNoServicesMessage() {
  if (state_ == NO_SERVICES)
    return;
  state_ = NO_SERVICES;

  ClearContents();
  views::GridLayout* layout = new views::GridLayout(contents_);
  layout->set_minimum_size(gfx::Size(WebIntentPicker::kWindowMinWidth, 0));
  const int kContentBuiltinBottomPadding = 3;
  layout->SetInsets(chrome_style::kCloseButtonPadding,
                    0,
                    chrome_style::kClientBottomPadding -
                        kContentBuiltinBottomPadding,
                    0);
  contents_->SetLayoutManager(layout);

  enum GridLayoutColumnSets {
    HEADER_ROW,
    CONTENT_ROW,
  };
  views::ColumnSet* header_cs = layout->AddColumnSet(HEADER_ROW);
  header_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);
  header_cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0); // Title
  header_cs->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                       GridLayout::USE_PREF, 0, 0); // Close button
  header_cs->AddPaddingColumn(
      0, chrome_style::kCloseButtonPadding);

  views::ColumnSet* content_cs = layout->AddColumnSet(CONTENT_ROW);
  content_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);
  content_cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0); // Body
  content_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);

  // Header
  layout->StartRow(0, HEADER_ROW);
  views::Label* title = CreateTitleLabel();
  title->SetText(l10n_util::GetStringUTF16(
      IDS_INTENT_PICKER_NO_SERVICES_TITLE));
  layout->AddView(title);

  views::ImageButton* close_button = CreateCloseButton(this);
  layout->AddView(close_button);
  close_button->SetVisible(use_close_button_);

  // Content
  const int kHeaderBuiltinBottomPadding = 4;
  const int kContentBuiltinTopPadding = 5;
  layout->AddPaddingRow(0,
                        chrome_style::kRowPadding -
                            kHeaderBuiltinBottomPadding -
                            kContentBuiltinTopPadding);
  layout->StartRow(0, CONTENT_ROW);
  views::Label* body = CreateLabel();
  body->SetMultiLine(true);
  body->SetText(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_NO_SERVICES));
  layout->AddView(body);

  int height = contents_->GetHeightForWidth(WebIntentPicker::kWindowMinWidth);
  contents_->SetSize(gfx::Size(WebIntentPicker::kWindowMinWidth, height));
  contents_->Layout();
}

void WebIntentPickerViews::OnInlineDispositionWebContentsLoaded(
    content::WebContents* web_contents) {
  if (state_ == INLINE_SERVICE)
    return;
  state_ = INLINE_SERVICE;

  // Replace the picker with the inline disposition.
  ClearContents();
  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  grid_layout->set_minimum_size(gfx::Size(WebIntentPicker::kWindowMinWidth, 0));
  grid_layout->SetInsets(chrome_style::kCloseButtonPadding, 0,
                         chrome_style::kClientBottomPadding, 0);
  contents_->SetLayoutManager(grid_layout);

  enum GridLayoutColumnSets {
    HEADER_ROW,
    SEPARATOR_ROW,
    WEB_CONTENTS_ROW,
  };
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(HEADER_ROW);
  header_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);
  header_cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0); // Icon, title, link.
  header_cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  header_cs->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                       GridLayout::USE_PREF, 0, 0); // Close button.
  header_cs->AddPaddingColumn(
      0, chrome_style::kCloseButtonPadding);

  views::ColumnSet* sep_cs = grid_layout->AddColumnSet(SEPARATOR_ROW);
  sep_cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                    GridLayout::USE_PREF, 0, 0); // Separator.

  views::ColumnSet* contents_cs = grid_layout->AddColumnSet(WEB_CONTENTS_ROW);
  contents_cs->AddPaddingColumn(0, 1);
  contents_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 1,
                         GridLayout::USE_PREF, 0, 0); // Web contents.
  contents_cs->AddPaddingColumn(0, 1);

  // Header.
  grid_layout->StartRow(0, HEADER_ROW);

  const WebIntentPickerModel::InstalledService* service =
      model_->GetInstalledServiceWithURL(model_->inline_disposition_url());

  if (!inline_service_icon_)
    inline_service_icon_ = new views::ImageView();
  inline_service_icon_->SetImage(service->favicon.ToImageSkia());

  views::Label* title = CreateLabel();
  title->SetText(ui::ElideText(
      service->title, title->font(), kTitleLinkMaxWidth, ui::ELIDE_AT_END));

  if (!choose_another_service_link_)
    choose_another_service_link_ = CreateLink();
  choose_another_service_link_->SetText(l10n_util::GetStringUTF16(
      IDS_INTENT_PICKER_USE_ALTERNATE_SERVICE));
  choose_another_service_link_->set_listener(this);

  grid_layout->AddView(CreateInlineDispositionHeader(
      inline_service_icon_, title, choose_another_service_link_));
  choose_another_service_link_->SetVisible(IsUseAnotherServiceVisible(model_));

  views::ImageButton* close_button = CreateCloseButton(this);
  grid_layout->AddView(close_button);
  close_button->SetVisible(use_close_button_);

  // Separator.
  const int kHeaderBuiltinBottomPadding = 4;
  grid_layout->AddPaddingRow(0,
                             chrome_style::kRowPadding -
                                 kHeaderBuiltinBottomPadding);
  grid_layout->StartRow(0, SEPARATOR_ROW);
  grid_layout->AddView(new views::Separator());

  // Inline web contents.
  const int kSeparatorBottomPadding = 3;
  grid_layout->AddPaddingRow(0, kSeparatorBottomPadding);
  grid_layout->StartRow(0, WEB_CONTENTS_ROW);
  grid_layout->AddView(webview_);

  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnModelChanged(WebIntentPickerModel* model) {
  if (state_ == WAITING && !model->IsWaitingForSuggestions())
    UpdateContents();

  if (choose_another_service_link_) {
    choose_another_service_link_->SetVisible(IsUseAnotherServiceVisible(model));
    contents_->Layout();
    SizeToContents();
  }

  if (extensions_)
    RefreshExtensions();
}

void WebIntentPickerViews::OnFaviconChanged(WebIntentPickerModel* model,
                                            size_t index) {
  // TODO(groby): Update favicons on extensions_;
  if (inline_service_icon_)
    RefreshInlineServiceIcon();
  if (extensions_)
    RefreshExtensions();
}

void WebIntentPickerViews::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const std::string& extension_id) {
  OnFaviconChanged(model, -1);
}

void WebIntentPickerViews::OnInlineDisposition(
    const string16&, const GURL& url) {
  DCHECK(delegate_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!webview_)
    webview_ = new views::WebView(profile);

  inline_web_contents_.reset(
      delegate_->CreateWebContentsForInlineDisposition(profile, url));

  // Does not take ownership, so we keep a scoped_ptr
  // for the WebContents locally.
  webview_->SetWebContents(inline_web_contents_.get());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  inline_disposition_delegate_.reset(
      new WebIntentInlineDispositionDelegate(this, inline_web_contents_.get(),
                                             browser));

  inline_web_contents_->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());

  // Disable all buttons.
  // TODO(groby): Add throbber for inline dispo - see http://crbug.com/142519.
  if (extensions_)
    extensions_->SetEnabled(false);
  if (more_suggestions_link_)
    more_suggestions_link_->SetEnabled(false);
  contents_->Layout();
}

void WebIntentPickerViews::OnExtensionInstallClicked(
    const std::string& extension_id) {
  DCHECK(delegate_);
  can_close_ = false;
  extensions_->StartThrobber(extension_id);
  more_suggestions_link_->SetEnabled(false);
  contents_->Layout();
  delegate_->OnExtensionInstallRequested(extension_id);
}

void WebIntentPickerViews::OnExtensionLinkClicked(
    const std::string& extension_id,
    WindowOpenDisposition disposition) {
  DCHECK(delegate_);
  delegate_->OnExtensionLinkClicked(extension_id, disposition);
}

void WebIntentPickerViews::OnActionButtonClicked(
    IntentRowView::ActionType type, size_t tag) {
  DCHECK(delegate_);
  DCHECK_EQ(IntentRowView::ACTION_INVOKE, type);
  const WebIntentPickerModel::InstalledService& service =
      model_->GetInstalledServiceAt(tag);
  delegate_->OnServiceChosen(service.url, service.disposition,
                             WebIntentPickerDelegate::kEnableDefaults);
}

void WebIntentPickerViews::UpdateContents() {
  if (model_ && model_->IsInlineDisposition())
    return;

  if (model_ && model_->IsWaitingForSuggestions()) {
    ShowWaitingForSuggestions();
  } else if (model_ && (model_->GetInstalledServiceCount() ||
                        model_->GetSuggestedExtensionCount())) {
    ShowAvailableServices();
  } else {
    ShowNoServicesMessage();
  }
  SizeToContents();
}

void WebIntentPickerViews::ShowWaitingForSuggestions() {
  if (state_ == WAITING)
    return;
  state_ = WAITING;
  ClearContents();
  contents_->SetLayoutManager(new views::FillLayout());
  waiting_view_ = new WaitingView(this, use_close_button_);
  contents_->AddChildView(waiting_view_);
  int height = contents_->GetHeightForWidth(kWindowMinWidth);
  contents_->SetSize(gfx::Size(kWindowMinWidth, height));
  contents_->Layout();
}

const string16 WebIntentPickerViews::GetActionTitle() {
  return action_text_.empty() ?
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE) :
      action_text_;
}

void WebIntentPickerViews::ShowAvailableServices() {
  ClearContents();
  state_ = LIST_SERVICES;
  extensions_ = new IntentsView(model_, this);
  gfx::Size min_size = gfx::Size(extensions_->AdjustWidth(kWindowMinWidth), 0);

  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  grid_layout->set_minimum_size(min_size);
  const int kIconBuiltinBottomPadding = 4;
  grid_layout->SetInsets(chrome_style::kCloseButtonPadding,
                         0,
                         chrome_style::kClientBottomPadding -
                             kIconBuiltinBottomPadding,
                         0);
  contents_->SetLayoutManager(grid_layout);

  enum GridLayoutColumnSets {
    HEADER_ROW,
    CONTENT_ROW,
  };
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(HEADER_ROW);
  header_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);
  header_cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0); // Action title
  header_cs->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                       GridLayout::USE_PREF, 0, 0); // Close button
  header_cs->AddPaddingColumn(
      0, chrome_style::kCloseButtonPadding);

  views::ColumnSet* content_cs = grid_layout->AddColumnSet(CONTENT_ROW);
  content_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);
  content_cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0); // Content.
  content_cs->AddPaddingColumn(
      0, chrome_style::kHorizontalPadding);

  // Header.
  grid_layout->StartRow(0, HEADER_ROW);
  if (!action_label_)
    action_label_ = CreateTitleLabel();
  action_label_->SetText(GetActionTitle());
  grid_layout->AddView(action_label_);

  views::ImageButton* close_button = CreateCloseButton(this);
  grid_layout->AddView(close_button);
  close_button->SetVisible(use_close_button_);

  // Extensions.
  const int kHeaderBuiltinBottomPadding = 4;
  grid_layout->AddPaddingRow(0,
                             chrome_style::kRowPadding -
                                 kHeaderBuiltinBottomPadding);
  grid_layout->StartRow(0, CONTENT_ROW);
  grid_layout->AddView(extensions_);


  // Row with "more suggestions" link.
  const int kIconBuiltinTopPadding = 6;
  grid_layout->AddPaddingRow(0,
                             chrome_style::kRowPadding -
                                 kIconBuiltinTopPadding);
  grid_layout->StartRow(0, CONTENT_ROW);
  views::View* more_view = new views::View();
  more_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kIconTextPadding));
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_WEBSTORE_ICON_16));
  more_view->AddChildView(icon);
  if (!more_suggestions_link_)
    more_suggestions_link_ = CreateLink();
  more_suggestions_link_->SetText(
      l10n_util::GetStringUTF16(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE));
  more_suggestions_link_->set_listener(this);
  more_view->AddChildView(more_suggestions_link_);
  grid_layout->AddView(more_view, 1, 1,
                       GridLayout::LEADING, GridLayout::CENTER);

  contents_->Layout();
}

void WebIntentPickerViews::ResetContents() {
  // Abandon both web contents and webview.
  webview_->SetWebContents(NULL);
  inline_web_contents_.reset();
  webview_ = NULL;

  // Re-initialize the UI.
  UpdateContents();

  // Restore previous state.
  if (extensions_)
    extensions_->Update();
  if (action_label_)
    action_label_->SetText(action_text_);
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::SizeToContents() {
  gfx::Size client_size = contents_->GetPreferredSize();
  gfx::Rect client_bounds(client_size);
  gfx::Rect new_window_bounds = window_->non_client_view()->frame_view()->
      GetWindowBoundsForClientBounds(client_bounds);
  window_->CenterWindow(new_window_bounds.size());
}

void WebIntentPickerViews::ClearContents() {
  DCHECK(contents_);
  // The call RemoveAllChildViews(true) deletes all children of |contents|. If
  // we do not set our weak pointers to NULL, then they will continue to point
  // to where the deleted objects used to be, i.e. unitialized memory. This
  // would cause hard-to-explain crashes.
  contents_->RemoveAllChildViews(true);
  action_label_ = NULL;
  extensions_ = NULL;
  more_suggestions_link_ = NULL;
  inline_service_icon_ = NULL;
  choose_another_service_link_ = NULL;
}

void WebIntentPickerViews::RefreshInlineServiceIcon() {
  DCHECK(inline_service_icon_);
  const WebIntentPickerModel::InstalledService* inline_service =
      model_->GetInstalledServiceWithURL(model_->inline_disposition_url());
  if (inline_service)
    inline_service_icon_->SetImage(inline_service->favicon.ToImageSkia());
}

void WebIntentPickerViews::RefreshExtensions() {
  DCHECK(extensions_);
  extensions_->Update();
  contents_->Layout();
  SizeToContents();
}
