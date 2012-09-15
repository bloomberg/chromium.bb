// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/shared_resources.h"
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
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                         rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X));
  close_button->SetImage(views::CustomButton::BS_HOT,
                         rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X_HOVER));
  close_button->SetImage(views::CustomButton::BS_PUSHED,
                         rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X_HOVER));
  return close_button;
}
// SarsView -------------------------------------------------------------------

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

// WaitingView ----------------------------------------------------------
class WaitingView : public views::View {
 public:
  WaitingView(views::ButtonListener* listener, bool use_close_button);

 private:
  DISALLOW_COPY_AND_ASSIGN(WaitingView);
};

WaitingView::WaitingView(views::ButtonListener* listener,
                         bool use_close_button) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(WebIntentPicker::kWindowWidth, 0));
  layout->SetInsets(WebIntentPicker::kContentAreaBorder,
                    WebIntentPicker::kContentAreaBorder,
                    WebIntentPicker::kContentAreaBorder,
                    WebIntentPicker::kContentAreaBorder);
  SetLayoutManager(layout);

  views::ColumnSet* cs = layout->AddColumnSet(0);
  views::ColumnSet* header_cs = NULL;
  if (use_close_button) {
    header_cs = layout->AddColumnSet(1);
    header_cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
    header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                         GridLayout::USE_PREF, 0, 0);  // Close Button.
  }
  cs->AddPaddingColumn(0, views::kPanelHorizIndentation);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER,
                1, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kPanelHorizIndentation);

  // Create throbber.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* frames =
      rb.GetImageNamed(IDR_SPEECH_INPUT_SPINNER).ToImageSkia();
  views::Throbber* throbber = new views::Throbber(kThrobberFrameTimeMs, true);
  throbber->SetFrames(frames);
  throbber->Start();

  // Create text.
  views::Label* label = new views::Label();
  label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  label->SetFont(rb.GetFont(ui::ResourceBundle::MediumBoldFont));
  label->SetText(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_WAIT_FOR_CWS));

  // Layout the view.
  if (use_close_button) {
    layout->StartRow(0, 1);
    layout->AddView(CreateCloseButton(listener));
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlLargeVerticalSpacing);
  layout->StartRow(0, 0);
  layout->AddView(throbber);
  layout->AddPaddingRow(0, views::kUnrelatedControlLargeVerticalSpacing);
  layout->StartRow(0, 0);
  layout->AddView(label);
  layout->AddPaddingRow(0, views::kUnrelatedControlLargeVerticalSpacing);
}

// SuggestedExtensionsLayout ---------------------------------------------------

// TODO(groby): Extremely fragile code, relies on order and number of fields.
// Would probably be better off as GridLayout or similar. Also see review
// comments on http://codereview.chromium.org/10909183

// A LayoutManager used by a row of the IntentsView. It is similar
// to a BoxLayout, but it right aligns the rightmost view (which is the install
// button). It also uses the maximum height of all views in the row as a
// preferred height so it doesn't change when the install button is hidden.
class SuggestedExtensionsLayout : public views::LayoutManager {
 public:
  SuggestedExtensionsLayout();
  virtual ~SuggestedExtensionsLayout();

  // Implementation of views::LayoutManager.
  virtual void Layout(views::View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestedExtensionsLayout);
};

SuggestedExtensionsLayout::SuggestedExtensionsLayout() {
}

SuggestedExtensionsLayout::~SuggestedExtensionsLayout() {
}

void SuggestedExtensionsLayout::Layout(views::View* host) {
  gfx::Rect child_area(host->GetLocalBounds());
  child_area.Inset(host->GetInsets());
  int x = child_area.x();
  int y = child_area.y();

  for (int i = 0; i < host->child_count(); ++i) {
    views::View* child = host->child_at(i);
    if (!child->visible())
      continue;
    gfx::Size size(child->GetPreferredSize());
    gfx::Rect child_bounds(x, y, size.width(), child_area.height());
    if (i == host->child_count() - 1) {
      // Last child (the install button) should be right aligned.
      child_bounds.set_x(std::max(child_area.width() - size.width(), x));
    } else if (i == 1) {
      // Label is considered fixed width, to align ratings widget.
      DCHECK_LE(size.width(), WebIntentPicker::kTitleLinkMaxWidth);
      x += WebIntentPicker::kTitleLinkMaxWidth +
          views::kRelatedControlHorizontalSpacing;
    } else {
      x += size.width() + views::kRelatedControlHorizontalSpacing;
    }
    // Clamp child view bounds to |child_area|.
    child->SetBoundsRect(child_bounds.Intersect(child_area));
  }
}

gfx::Size SuggestedExtensionsLayout::GetPreferredSize(views::View* host) {
  int width = 0;
  int height = 0;
  for (int i = 0; i < host->child_count(); ++i) {
    views::View* child = host->child_at(i);
    gfx::Size size(child->GetPreferredSize());
    // The preferred height includes visible and invisible children. This
    // prevents jank when a child is hidden.
    height = std::max(height, size.height());
    if (!child->visible())
      continue;
    if (i != 0)
      width += views::kRelatedControlHorizontalSpacing;
  }
  gfx::Insets insets(host->GetInsets());
  return gfx::Size(width + insets.width(), height + insets.height());
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
    virtual void OnExtensionInstallClicked(const string16& extension_id) = 0;

    // Called when the user clicks the extension title link.
    virtual void OnExtensionLinkClicked(
        const string16& extension_id,
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

  void MarkBusy(const string16& extension_id);

  // Stop the throbber for this row, and show the star rating and the install
  // button.
  void StopThrobber();

 protected:
  virtual void OnEnabledChanged() OVERRIDE;

  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

 private:
  IntentRowView(ActionType type, size_t tag);

  // Identifier for the suggested extension displayed in this row.
  string16 extension_id_;

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
    : type_(type), tag_(tag) {}

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

  int message_id = 0;
  const gfx::ImageSkia* icon = NULL;
  StarsView* stars = NULL;
  views::Label* label = NULL;
  IntentRowView* view;
  if (service != NULL) {
    view = new IntentRowView(ACTION_INVOKE, tag);
    message_id = IDS_INTENT_PICKER_SELECT_INTENT;
    icon = service->favicon.ToImageSkia();
    label = new views::Label(elided_title);
  } else {
    view = new IntentRowView(ACTION_INSTALL, tag);
    view->extension_id_ = extension->id;
    message_id = IDS_INTENT_PICKER_INSTALL_EXTENSION;
    icon = extension->icon.ToImageSkia();
    views::Link* link = new views::Link(elided_title);
    link->set_listener(view);
    label = link;
    stars = new StarsView(extension->average_rating);
  }

  DCHECK(message_id > 0);
  view->delegate_ = delegate;

  view->SetLayoutManager(new SuggestedExtensionsLayout);

  view->icon_ = new views::ImageView();

  view->icon_->SetImage(icon);
  view->AddChildView(view->icon_);

  view->title_link_ = label;
  view->AddChildView(view->title_link_);


  if (stars != NULL) {
    view->stars_ = stars;
    view->AddChildView(view->stars_);
  }

  view->install_button_ = new ThrobberNativeTextButton(
      view, l10n_util::GetStringUTF16(message_id));
  view->install_button_->set_preferred_width(preferred_width);
  view->AddChildView(view->install_button_);

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

void IntentRowView::MarkBusy(const string16& extension_id) {
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
  install_button_->SetText(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_INSTALL_EXTENSION));
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
  void StartThrobber(const string16& extension_id);

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

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, views::kRelatedControlVerticalSpacing);
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

void IntentsView::StartThrobber(const string16& extension_id) {
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
                             public views::DialogDelegate,
                             public views::LinkListener,
                             public WebIntentPicker,
                             public WebIntentPickerModelObserver,
                             public IntentRowView::Delegate {
 public:
  WebIntentPickerViews(TabContents* tab_contents,
                       WebIntentPickerDelegate* delegate,
                       WebIntentPickerModel* model);
  virtual ~WebIntentPickerViews();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::DialogDelegate implementation.
  virtual void WindowClosing() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;

  // LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;
  virtual void SetActionString(const string16& action) OVERRIDE;
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE;
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) OVERRIDE;
  virtual void OnPendingAsyncCompleted() OVERRIDE;
  virtual void OnInlineDispositionWebContentsLoaded(
      content::WebContents* web_contents) OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnExtensionIconChanged(WebIntentPickerModel* model,
                                      const string16& extension_id) OVERRIDE;
  virtual void OnInlineDisposition(const string16& title,
                                   const GURL& url) OVERRIDE;

  // SuggestedExtensionsRowView::Delegate implementation.
  virtual void OnExtensionInstallClicked(const string16& extension_id) OVERRIDE;
  virtual void OnExtensionLinkClicked(
      const string16& extension_id,
      WindowOpenDisposition disposition) OVERRIDE;
  virtual void OnActionButtonClicked(
      IntentRowView::ActionType type,
      size_t tag) OVERRIDE;

 private:
  // Initialize the contents of the picker. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // Initialize the main contents of the picker. (Suggestions, services).
  void InitMainContents();

  // Restore the contents of the picker to the initial contents.
  void ResetContents();

  // Resize the constrained window to the size of its contents.
  void SizeToContents();

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // A weak pointer to the picker model.
  WebIntentPickerModel* model_;

  // A weak pointer to the action string label.
  // Created locally, owned by Views.
  views::Label* action_label_;

  // A weak pointer to the header label for the extension suggestions.
  // Created locally, owned by Views.
  views::Label* suggestions_label_;

  // A weak pointer to the intents view.
  // Created locally, owned by Views view hierarchy.
  IntentsView* extensions_;

  // Delegate for inline disposition tab contents.
  scoped_ptr<WebIntentInlineDispositionDelegate> inline_disposition_delegate_;

  // A weak pointer to the TabContents this picker is in.
  TabContents* tab_contents_;

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

  // A weak pointer to the choose another service link.
  // Created locally, owned by Views.
  views::Link* choose_another_service_link_;

  // Weak pointer to "Waiting for CWS" display. Owned by parent view.
  WaitingView* waiting_view_;

  // Set to true when displaying the inline disposition web contents. Used to
  // prevent laying out the inline disposition widgets twice.
  bool displaying_web_contents_;

  // The text for the current action.
  string16 action_text_;

  // Ownership of the WebContents we are displaying in the inline disposition.
  scoped_ptr<WebContents> inline_web_contents_;

  // Indicate if dialog should display its own close button.
  // TODO(groby): Only relevant until new ConstrainedWindow is implemented,
  // from then on always true.
  bool use_close_button_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerViews);
};

// static
WebIntentPicker* WebIntentPicker::Create(TabContents* tab_contents,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerViews(tab_contents, delegate, model);
}

WebIntentPickerViews::WebIntentPickerViews(TabContents* tab_contents,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
    : delegate_(delegate),
      model_(model),
      action_label_(NULL),
      suggestions_label_(NULL),
      extensions_(NULL),
      tab_contents_(tab_contents),
      webview_(new views::WebView(tab_contents->profile())),
      contents_(NULL),
      window_(NULL),
      more_suggestions_link_(NULL),
      choose_another_service_link_(NULL),
      waiting_view_(NULL),
      displaying_web_contents_(false) {
  use_close_button_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableFramelessConstrainedDialogs);

  model_->set_observer(this);
  InitContents();

  // Show the dialog.
  window_ = new ConstrainedWindowViews(tab_contents, this);
}

WebIntentPickerViews::~WebIntentPickerViews() {
  model_->set_observer(NULL);
}

void WebIntentPickerViews::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  delegate_->OnPickerClosed();
}

void WebIntentPickerViews::WindowClosing() {
  delegate_->OnClosing();
  delegate_->OnPickerClosed();
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

int WebIntentPickerViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void WebIntentPickerViews::LinkClicked(views::Link* source, int event_flags) {
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
  window_->CloseConstrainedWindow();
}

void WebIntentPickerViews::SetActionString(const string16& action) {
  action_text_ = action;

  if (action_label_)
    action_label_->SetText(action);
}

void WebIntentPickerViews::OnExtensionInstallSuccess(const std::string& id) {
}

void WebIntentPickerViews::OnExtensionInstallFailure(const std::string& id) {
  extensions_->StopThrobber();
  more_suggestions_link_->SetEnabled(true);
  contents_->Layout();

  // TODO(binji): What to display to user on failure?
}

void WebIntentPickerViews::OnInlineDispositionAutoResize(
    const gfx::Size& size) {
  webview_->SetPreferredSize(size);
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnPendingAsyncCompleted() {
  // Requests to both the WebIntentService and the Chrome Web Store have
  // completed. If there are any services, installed or suggested, there's
  // nothing to do.
  if (model_->GetInstalledServiceCount() ||
      model_->GetSuggestedExtensionCount())
    return;

  // If there are no installed or suggested services at this point,
  // inform the user about it.
  contents_->RemoveAllChildViews(true);
  more_suggestions_link_ = NULL;

  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(grid_layout);

  grid_layout->SetInsets(kContentAreaBorder, kContentAreaBorder,
                         kContentAreaBorder, kContentAreaBorder);
  views::ColumnSet* main_cs = grid_layout->AddColumnSet(0);
  main_cs->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                     GridLayout::USE_PREF, 0, 0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  grid_layout->StartRow(0, 0);
  views::Label* header = new views::Label();
  header->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  header->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  header->SetText(l10n_util::GetStringUTF16(
      IDS_INTENT_PICKER_NO_SERVICES_TITLE));
  grid_layout->AddView(header);

  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  grid_layout->StartRow(0, 0);
  views::Label* body = new views::Label();
  body->SetMultiLine(true);
  body->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  body->SetText(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_NO_SERVICES));
  grid_layout->AddView(body);

  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnInlineDispositionWebContentsLoaded(
    content::WebContents* web_contents) {
  if (displaying_web_contents_)
    return;

  // Replace the picker with the inline disposition.
  contents_->RemoveAllChildViews(true);
  more_suggestions_link_ = NULL;

  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(grid_layout);

  grid_layout->SetInsets(kContentAreaBorder, kContentAreaBorder,
                         kContentAreaBorder, kContentAreaBorder);
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(0);
  header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Icon.
  header_cs->AddPaddingColumn(0, 4);
  header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Title.
  header_cs->AddPaddingColumn(0, 4);
  header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Link.
  header_cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  if (use_close_button_) {
    header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                         GridLayout::USE_PREF, 0, 0);  // Close Button.
  }

  views::ColumnSet* full_cs = grid_layout->AddColumnSet(1);
  full_cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                     GridLayout::USE_PREF, 0, 0);

  const WebIntentPickerModel::InstalledService* service =
      model_->GetInstalledServiceWithURL(model_->inline_disposition_url());

  // Header row.
  grid_layout->StartRow(0, 0);
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(service->favicon.ToImageSkia());
  grid_layout->AddView(icon);

  string16 elided_title = ui::ElideText(
      service->title, gfx::Font(), kTitleLinkMaxWidth, ui::ELIDE_AT_END);
  views::Label* title = new views::Label(elided_title);
  grid_layout->AddView(title);
  // Add link for "choose another service" if other suggestions are available
  // or if more than one (the current) service is installed.
  if (model_->GetInstalledServiceCount() > 1 ||
       model_->GetSuggestedExtensionCount()) {
    choose_another_service_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_INTENT_PICKER_USE_ALTERNATE_SERVICE));
    grid_layout->AddView(choose_another_service_link_);
    choose_another_service_link_->set_listener(this);
  }

  if (use_close_button_)
    grid_layout->AddView(CreateCloseButton(this));

  // Inline web contents row.
  grid_layout->StartRow(0, 1);
  grid_layout->AddView(webview_, 1, 1, GridLayout::CENTER,
                       GridLayout::CENTER, 0, 0);
  contents_->Layout();
  SizeToContents();
  displaying_web_contents_ = true;
}

void WebIntentPickerViews::OnModelChanged(WebIntentPickerModel* model) {
  if (waiting_view_ && !model->IsWaitingForSuggestions()) {
    InitMainContents();
  }
  if (suggestions_label_) {
    string16 label_text = model->GetSuggestionsLinkText();
    suggestions_label_->SetText(label_text);
    suggestions_label_->SetVisible(!label_text.empty());
  }

  if (extensions_)
    extensions_->Update();
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnFaviconChanged(WebIntentPickerModel* model,
                                            size_t index) {
  // TODO(groby): Update favicons on extensions_;
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const string16& extension_id) {
  if (extensions_)
    extensions_->Update();

  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnInlineDisposition(
    const string16&, const GURL& url) {
  if (!webview_)
    webview_ = new views::WebView(tab_contents_->profile());

  inline_web_contents_.reset(WebContents::Create(
      tab_contents_->profile(),
      tab_util::GetSiteInstanceForNewTab(tab_contents_->profile(), url),
      MSG_ROUTING_NONE, NULL));
  // Does not take ownership, so we keep a scoped_ptr
  // for the WebContents locally.
  webview_->SetWebContents(inline_web_contents_.get());
  Browser* browser = browser::FindBrowserWithWebContents(
      tab_contents_->web_contents());
  inline_disposition_delegate_.reset(
      new WebIntentInlineDispositionDelegate(this, inline_web_contents_.get(),
                                             browser));
  content::WebContents* web_contents = webview_->GetWebContents();

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);
  web_contents->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());

  // Disable all buttons.
  // TODO(groby): Add throbber for inline dispo - see http://crbug.com/142519.
  extensions_->SetEnabled(false);
  more_suggestions_link_->SetEnabled(false);
  contents_->Layout();
}

void WebIntentPickerViews::OnExtensionInstallClicked(
    const string16& extension_id) {
  extensions_->StartThrobber(extension_id);
  more_suggestions_link_->SetEnabled(false);
  contents_->Layout();
  delegate_->OnExtensionInstallRequested(UTF16ToUTF8(extension_id));
}

void WebIntentPickerViews::OnExtensionLinkClicked(
    const string16& extension_id,
    WindowOpenDisposition disposition) {
  delegate_->OnExtensionLinkClicked(UTF16ToUTF8(extension_id), disposition);
}

void WebIntentPickerViews::OnActionButtonClicked(
    IntentRowView::ActionType type, size_t tag) {
  DCHECK_EQ(IntentRowView::ACTION_INVOKE, type);
  const WebIntentPickerModel::InstalledService& service =
      model_->GetInstalledServiceAt(tag);
  delegate_->OnServiceChosen(service.url, service.disposition);
}

void WebIntentPickerViews::InitContents() {
  DCHECK(!contents_);
  contents_ = new views::View();

  if (model_ && model_->IsWaitingForSuggestions()) {
    contents_->RemoveAllChildViews(true);
    contents_->SetLayoutManager(new views::FillLayout());
    waiting_view_ = new WaitingView(this, use_close_button_);
    contents_->AddChildView(waiting_view_);
    contents_->Layout();
  } else {
    InitMainContents();
  }
}

void WebIntentPickerViews::InitMainContents() {
  DCHECK(contents_);
  enum {
    kHeaderRowColumnSet,  // Column set for header layout.
    kFullWidthColumnSet,  // Column set with a single full-width column.
    kIndentedFullWidthColumnSet,  // Single full-width column, indented.
  };

  contents_->RemoveAllChildViews(true);
  displaying_web_contents_ = false;

  extensions_ = new IntentsView(model_, this);

  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(grid_layout);

  grid_layout->set_minimum_size(
      gfx::Size(extensions_->AdjustWidth(kWindowWidth), 0));
  grid_layout->SetInsets(kContentAreaBorder, kContentAreaBorder,
                         kContentAreaBorder, kContentAreaBorder);
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(kHeaderRowColumnSet);
  header_cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0);  // Title.
  if (use_close_button_) {
    header_cs->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
    header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                         GridLayout::USE_PREF, 0, 0);  // Close Button.
  }

  views::ColumnSet* full_cs = grid_layout->AddColumnSet(kFullWidthColumnSet);
  full_cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                     GridLayout::USE_PREF, 0, 0);

  views::ColumnSet* indent_cs =
      grid_layout->AddColumnSet(kIndentedFullWidthColumnSet);
  indent_cs->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  indent_cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                       GridLayout::USE_PREF, 0, 0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Header row.
  grid_layout->StartRow(0, kHeaderRowColumnSet);
  action_label_ = new views::Label();
  action_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  action_label_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  grid_layout->AddView(action_label_);

  if (use_close_button_)
    grid_layout->AddView(CreateCloseButton(this));

  // Padding row.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Row with app suggestions label.
  grid_layout->StartRow(0, kIndentedFullWidthColumnSet);
  suggestions_label_ = new views::Label();
  suggestions_label_->SetVisible(false);
  suggestions_label_->SetMultiLine(true);
  suggestions_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  grid_layout->AddView(suggestions_label_);

  // Padding row.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Row with extension suggestions.
  grid_layout->StartRow(0, kIndentedFullWidthColumnSet);
  grid_layout->AddView(extensions_);

  // Padding row.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Row with "more suggestions" link.
  grid_layout->StartRow(0, kFullWidthColumnSet);
  more_suggestions_link_ = new views::Link(
    l10n_util::GetStringUTF16(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE));
  more_suggestions_link_->SetDisabledColor(kDisabledLinkColor);
  more_suggestions_link_->set_listener(this);
  grid_layout->AddView(more_suggestions_link_, 1, 1, GridLayout::LEADING,
                       GridLayout::CENTER);
}

void WebIntentPickerViews::ResetContents() {
  // Abandon both web contents and webview.
  webview_->SetWebContents(NULL);
  inline_web_contents_.reset();
  webview_ = NULL;

  // Re-initialize the UI.
  InitMainContents();

  // Restore previous state.
  extensions_->Update();
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
