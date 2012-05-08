// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
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
#include "grit/ui_resources_standard.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
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

// TODO(binji): The current constrained dialog implementation already has a
// close button. Remove this define and the #if checks below when we switch to
// the new implementation.
// #define USE_CLOSE_BUTTON

using content::WebContents;
using views::GridLayout;

namespace {

// The space in pixels between the top-level groups and the dialog border.
const int kContentAreaBorder = 12;

// The minimum size to display the constrained dialog.
const int kDialogMinWidth = 400;

// The maximum width in pixels of a suggested extension's title link.
const int kTitleLinkMaxWidth = 130;

// The color used to dim disabled elements.
const SkColor kHalfOpacityWhite = SkColorSetARGB(128, 255, 255, 255);

// The color used to display a disabled link.
const SkColor kDisabledLinkColor = SkColorSetRGB(128, 128, 128);

// The time between successive throbber frames in milliseconds.
const int kThrobberFrameTimeMs = 50;

// Enables or disables all child views of |view|.
void EnableChildViews(views::View* view, bool enabled) {
  for (int i = 0; i < view->child_count(); ++i) {
    views::View* child = view->child_at(i);
    child->SetEnabled(enabled);
  }
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
    image->SetImage(rb.GetBitmapNamed(
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
  void SetFrames(const SkBitmap* frames);

 protected:
  virtual const SkBitmap& GetImageToPaint() const OVERRIDE;

 private:
  // The timer callback to schedule painting this view.
  void Run();

  // Bitmap that contains the throbber frames.
  const SkBitmap* frames_;

  // The currently displayed frame, given to GetImageToPaint.
  mutable SkBitmap this_frame_;

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

  DISALLOW_COPY_AND_ASSIGN(ThrobberNativeTextButton);
};

ThrobberNativeTextButton::ThrobberNativeTextButton(
    views::ButtonListener* listener, const string16& text)
    : NativeTextButton(listener, text),
      frame_time_(base::TimeDelta::FromMilliseconds(kThrobberFrameTimeMs)),
      frame_count_(0),
      running_(false) {
  SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_THROBBER).ToSkBitmap());
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

void ThrobberNativeTextButton::SetFrames(const SkBitmap* frames) {
  frames_ = frames;
  DCHECK(frames_->width() > 0 && frames_->height() > 0);
  DCHECK(frames_->width() % frames_->height() == 0);
  frame_count_ = frames_->width() / frames_->height();
  PreferredSizeChanged();
}

const SkBitmap& ThrobberNativeTextButton::GetImageToPaint() const {
  if (!running_)
    return NativeTextButton::GetImageToPaint();

  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  const int current_frame =
      static_cast<int>(elapsed_time / frame_time_) % frame_count_;
  const int image_size = frames_->height();
  const int image_offset = current_frame * image_size;

  SkIRect subset_rect = SkIRect::MakeXYWH(image_offset, 0,
                                          image_size, image_size);
  frames_->extractSubset(&this_frame_, subset_rect);
  return this_frame_;
}

void ThrobberNativeTextButton::Run() {
  DCHECK(running_);

  SchedulePaint();
}

// ServiceButtonsView ----------------------------------------------------------

// A view that contains all service buttons (i.e. the installed services).
class ServiceButtonsView : public views::View,
                           public views::ButtonListener {
 public:
  class Delegate {
   public:
    // Called when a service button is clicked. |index| is the index of the
    // service button in the model.
    virtual void OnServiceButtonClicked(
        const WebIntentPickerModel::InstalledService& service) = 0;

   protected:
    virtual ~Delegate() {}
  };

  ServiceButtonsView(const WebIntentPickerModel* model, Delegate* delegate);
  virtual ~ServiceButtonsView();

  // Updates the service button view with new model data.
  void Update();

  // Start a throbber on the service button that will launch the service at
  // |url|.
  void StartThrobber(const GURL& url);

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  virtual gfx::Size GetPreferredSize() OVERRIDE;

 protected:
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  const WebIntentPickerModel* model_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ServiceButtonsView);
};

ServiceButtonsView::ServiceButtonsView(const WebIntentPickerModel* model,
                                       Delegate* delegate)
    : model_(model),
      delegate_(delegate) {
  Update();
}

ServiceButtonsView::~ServiceButtonsView() {
}

void ServiceButtonsView::Update() {
  RemoveAllChildViews(true);
  views::GridLayout* grid_layout = new views::GridLayout(this);
  SetLayoutManager(grid_layout);

  if (model_->GetInstalledServiceCount() == 0)
    return;

  views::ColumnSet* cs = grid_layout->AddColumnSet(0);
  cs->AddPaddingColumn(0, views::kPanelHorizIndentation);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                1, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kPanelHorizIndentation);

  // Spacing between header and service buttons.
  grid_layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  for (size_t i = 0; i < model_->GetInstalledServiceCount(); ++i) {
    const WebIntentPickerModel::InstalledService& service =
        model_->GetInstalledServiceAt(i);

    grid_layout->StartRow(0, 0);

    ThrobberNativeTextButton* button =
        new ThrobberNativeTextButton(this, service.title);
    button->set_alignment(views::TextButton::ALIGN_LEFT);
    button->SetTooltipText(UTF8ToUTF16(service.url.spec().c_str()));
    button->SetIcon(*service.favicon.ToSkBitmap());
    button->set_tag(static_cast<int>(i));
    grid_layout->AddView(button);
    grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Additional space to separate the buttons from the suggestions.
  grid_layout->AddPaddingRow(0, views::kPanelVerticalSpacing);
}

void ServiceButtonsView::StartThrobber(const GURL& url) {
  for (size_t i = 0; i < model_->GetInstalledServiceCount(); ++i) {
    const WebIntentPickerModel::InstalledService& service =
        model_->GetInstalledServiceAt(i);
    if (service.url != url)
      continue;

    ThrobberNativeTextButton* button =
        static_cast<ThrobberNativeTextButton*>(child_at(i));
    button->StartThrobber();
    return;
  }
}

void ServiceButtonsView::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  size_t index = static_cast<size_t>(sender->tag());
  delegate_->OnServiceButtonClicked(model_->GetInstalledServiceAt(index));
}

gfx::Size ServiceButtonsView::GetPreferredSize() {
  // If there are no service buttons, hide this view.
  if (model_->GetInstalledServiceCount() == 0)
    return gfx::Size();
  return GetLayoutManager()->GetPreferredSize(this);
}

void ServiceButtonsView::OnEnabledChanged() {
  EnableChildViews(this, enabled());
  View::OnEnabledChanged();
}

// SuggestedExtensionsLayout ---------------------------------------------------

// A LayoutManager used by a row of the SuggestedExtensionsView. It is similar
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

// SuggestedExtensionsRowView --------------------------------------------------

// A view for each row in the SuggestedExtensionsView. It displays information
// for a given SuggestedExtension.
class SuggestedExtensionsRowView : public views::View,
                                   public views::ButtonListener,
                                   public views::LinkListener {
 public:
  class Delegate {
   public:
    // Called when the user clicks the "Add to Chrome" button.
    virtual void OnExtensionInstallClicked(const string16& extension_id) = 0;

    // Called when the user clicks the extension title link.
    virtual void OnExtensionLinkClicked(const string16& extension_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  SuggestedExtensionsRowView(
      const WebIntentPickerModel::SuggestedExtension* extension,
      Delegate* delegate);
  virtual ~SuggestedExtensionsRowView();

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // LinkListener implementation.
  void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Start an animating throbber for this row, and hide the star rating and the
  // install button.
  void StartThrobber();

  // Stop the throbber for this row, and show the star rating and the install
  // button.
  void StopThrobber();

 protected:
  virtual void OnEnabledChanged() OVERRIDE;

  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

 private:
  // The extension to display in this row.
  const WebIntentPickerModel::SuggestedExtension* extension_;

  // A delegate to respond to button presses and clicked links.
  Delegate* delegate_;

  // The icon of the extension.
  views::ImageView* icon_;

  // The title of the extension, which links to the CWS detailed description of
  // this extension.
  views::Link* title_link_;

  // The star rating of this extension.
  StarsView* stars_;

  // A button to install the extension.
  ThrobberNativeTextButton* install_button_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedExtensionsRowView);
};

SuggestedExtensionsRowView::SuggestedExtensionsRowView(
    const WebIntentPickerModel::SuggestedExtension* extension,
    Delegate* delegate)
    : extension_(extension),
      delegate_(delegate) {
  SetLayoutManager(new SuggestedExtensionsLayout);

  icon_ = new views::ImageView();
  icon_->SetImage(extension_->icon.ToSkBitmap());
  AddChildView(icon_);

  string16 elided_title = ui::ElideText(
      extension_->title, gfx::Font(), kTitleLinkMaxWidth, ui::ELIDE_AT_END);
  title_link_ = new views::Link(elided_title);
  title_link_->set_listener(this);
  AddChildView(title_link_);

  stars_ = new StarsView(extension_->average_rating);
  AddChildView(stars_);

  install_button_= new ThrobberNativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_INTENT_PICKER_INSTALL_EXTENSION));
  AddChildView(install_button_);
}

SuggestedExtensionsRowView::~SuggestedExtensionsRowView() {
}

void SuggestedExtensionsRowView::ButtonPressed(views::Button* sender,
                                               const views::Event& event) {
  delegate_->OnExtensionInstallClicked(extension_->id);
}

void SuggestedExtensionsRowView::LinkClicked(views::Link* source,
                                             int event_flags) {
  delegate_->OnExtensionLinkClicked(extension_->id);
}

void SuggestedExtensionsRowView::StartThrobber() {
  install_button_->StartThrobber();
  install_button_->SetText(string16());
}

void SuggestedExtensionsRowView::StopThrobber() {
  install_button_->StopThrobber();
  install_button_->SetText(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_INSTALL_EXTENSION));
}

void SuggestedExtensionsRowView::OnEnabledChanged() {
  title_link_->SetEnabled(enabled());
  stars_->SetEnabled(enabled());
  install_button_->SetEnabled(enabled());
  View::OnEnabledChanged();
  Layout();
}

void SuggestedExtensionsRowView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  if (!enabled())
    canvas->FillRect(GetLocalBounds(), kHalfOpacityWhite);
}

// SuggestedExtensionsView -----------------------------------------------------

// A view that contains suggested extensions from the Chrome Web Store that
// provide an intent service matching the action/type pair.
class SuggestedExtensionsView : public views::View {
 public:
  SuggestedExtensionsView(const WebIntentPickerModel* model,
                          SuggestedExtensionsRowView::Delegate* delegate);

  virtual ~SuggestedExtensionsView();

  void Clear();

  // Update the view to the new model data.
  void Update();

  // Show the install throbber for the row containing |extension_id|. This
  // function also hides hides and disables other buttons and links.
  void StartThrobber(const string16& extension_id);

  // Hide the install throbber. This function re-enables all buttons and links.
  void StopThrobber();

 protected:
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  const WebIntentPickerModel* model_;
  SuggestedExtensionsRowView::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedExtensionsView);
};

SuggestedExtensionsView::SuggestedExtensionsView(
    const WebIntentPickerModel* model,
    SuggestedExtensionsRowView::Delegate* delegate)
    : model_(model),
      delegate_(delegate) {
  Update();
}

SuggestedExtensionsView::~SuggestedExtensionsView() {
}

void SuggestedExtensionsView::Clear() {
  RemoveAllChildViews(true);
}

void SuggestedExtensionsView::Update() {
  Clear();

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, views::kRelatedControlVerticalSpacing);
  SetLayoutManager(layout);

  for (size_t i = 0; i < model_->GetSuggestedExtensionCount(); ++i) {
    const WebIntentPickerModel::SuggestedExtension& extension =
        model_->GetSuggestedExtensionAt(i);
    AddChildView(
        new SuggestedExtensionsRowView(&extension, delegate_));
  }
}

void SuggestedExtensionsView::StartThrobber(const string16& extension_id) {
  for (size_t i = 0; i < model_->GetSuggestedExtensionCount(); ++i) {
    SuggestedExtensionsRowView* row =
        static_cast<SuggestedExtensionsRowView*>(child_at(i));
    bool extension_id_matches =
        model_->GetSuggestedExtensionAt(i).id == extension_id;
    row->SetEnabled(extension_id_matches);
    if (extension_id_matches)
      row->StartThrobber();
  }
}

void SuggestedExtensionsView::StopThrobber() {
  for (size_t i = 0; i < model_->GetSuggestedExtensionCount(); ++i) {
    SuggestedExtensionsRowView* row =
        static_cast<SuggestedExtensionsRowView*>(child_at(i));
    row->SetEnabled(true);
    row->StopThrobber();
  }
}

void SuggestedExtensionsView::OnEnabledChanged() {
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
                             public ServiceButtonsView::Delegate,
                             public SuggestedExtensionsRowView::Delegate {
 public:
  WebIntentPickerViews(TabContentsWrapper* tab_contents,
                       WebIntentPickerDelegate* delegate,
                       WebIntentPickerModel* model);
  virtual ~WebIntentPickerViews();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

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
  virtual void OnInlineDispositionWebContentsLoaded(
      content::WebContents* web_contents) OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnExtensionIconChanged(WebIntentPickerModel* model,
                                      const string16& extension_id) OVERRIDE;
  virtual void OnInlineDisposition(WebIntentPickerModel* model,
                                   const GURL& url) OVERRIDE;

  // ServiceButtonsView::Delegate implementation.
  virtual void OnServiceButtonClicked(
      const WebIntentPickerModel::InstalledService& service) OVERRIDE;

  // SuggestedExtensionsRowView::Delegate implementation.
  virtual void OnExtensionInstallClicked(const string16& extension_id) OVERRIDE;
  virtual void OnExtensionLinkClicked(const string16& extension_id) OVERRIDE;

 private:
  // Initialize the contents of the picker. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // Restore the contents of the picker to the initial contents.
  void ResetContents();

  // Resize the constrained window to the size of its contents.
  void SizeToContents();

#if defined(USE_CLOSE_BUTTON)
  // Returns a new close button.
  views::ImageButton* CreateCloseButton();
#endif

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // A weak pointer to the picker model.
  WebIntentPickerModel* model_;

  // A weak pointer to the service button view.
  // Created locally, owned by Views.
  ServiceButtonsView* service_buttons_;

  // A weak pointer to the action string label.
  // Created locally, owned by Views.
  views::Label* action_label_;

  // A weak pointer to the header label for the extension suggestions.
  // Created locally, owned by Views.
  views::Label* suggestions_label_;

  // A weak pointer to the extensions view.
  // Created locally, owned by Views.
  SuggestedExtensionsView* extensions_;

  // Delegate for inline disposition tab contents.
  scoped_ptr<WebIntentInlineDispositionDelegate> inline_disposition_delegate_;

  // A weak pointer to the wrapper of the WebContents this picker is in.
  TabContentsWrapper* wrapper_;

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

  // Set to true when displaying the inline disposition web contents. Used to
  // prevent laying out the inline disposition widgets twice.
  bool displaying_web_contents_;

  // The text for the current action.
  string16 action_text_;

  // Ownership of the WebContents we are displaying in the inline disposition.
  scoped_ptr<WebContents> inline_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerViews);
};

// static
WebIntentPicker* WebIntentPicker::Create(TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  WebIntentPickerViews* picker =
      new WebIntentPickerViews(wrapper, delegate, model);

  return picker;
}

WebIntentPickerViews::WebIntentPickerViews(TabContentsWrapper* wrapper,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
    : delegate_(delegate),
      model_(model),
      service_buttons_(NULL),
      action_label_(NULL),
      suggestions_label_(NULL),
      extensions_(NULL),
      wrapper_(wrapper),
      webview_(new views::WebView(wrapper->profile())),
      contents_(NULL),
      window_(NULL),
      more_suggestions_link_(NULL),
      choose_another_service_link_(NULL),
      displaying_web_contents_(false) {
  model_->set_observer(this);
  InitContents();

  // Show the dialog.
  window_ = new ConstrainedWindowViews(wrapper, this);
}

WebIntentPickerViews::~WebIntentPickerViews() {
  model_->set_observer(NULL);
}

void WebIntentPickerViews::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
#if defined(USE_CLOSE_BUTTON)
  delegate_->OnPickerCancelled();
#endif
}

void WebIntentPickerViews::WindowClosing() {
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

int WebIntentPickerViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void WebIntentPickerViews::LinkClicked(views::Link* source, int event_flags) {
  if (source == more_suggestions_link_) {
    delegate_->OnSuggestionsLinkClicked();
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
  action_label_->SetText(action);
}

void WebIntentPickerViews::OnExtensionInstallSuccess(const std::string& id) {
}

void WebIntentPickerViews::OnExtensionInstallFailure(const std::string& id) {
  service_buttons_->SetEnabled(true);
  extensions_->StopThrobber();
  more_suggestions_link_->SetEnabled(true);
  contents_->Layout();

  // TODO(binji): What to display to user on failure?
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
#if defined(USE_CLOSE_BUTTON)
  header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Close Button.
#endif

  views::ColumnSet* full_cs = grid_layout->AddColumnSet(1);
  full_cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                     GridLayout::USE_PREF, 0, 0);

  const WebIntentPickerModel::InstalledService* service =
      model_->GetInstalledServiceWithURL(model_->inline_disposition_url());

  // Header row.
  grid_layout->StartRow(0, 0);
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(service->favicon.ToSkBitmap());
  grid_layout->AddView(icon);

  string16 elided_title = ui::ElideText(
      service->title, gfx::Font(), kTitleLinkMaxWidth, ui::ELIDE_AT_END);
  views::Label* title = new views::Label(elided_title);
  grid_layout->AddView(title);

  choose_another_service_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_USE_ALTERNATE_SERVICE));
  grid_layout->AddView(choose_another_service_link_);
  choose_another_service_link_->set_listener(this);

#if defined(USE_CLOSE_BUTTON)
  grid_layout->AddView(CreateCloseButton());
#endif

  // Inline web contents row.
  grid_layout->StartRow(0, 1);
  grid_layout->AddView(webview_, 1, 1, GridLayout::CENTER,
                       GridLayout::CENTER, kDialogMinWidth, 140);
  contents_->Layout();
  SizeToContents();
  displaying_web_contents_ = true;
}

void WebIntentPickerViews::OnModelChanged(WebIntentPickerModel* model) {
  if (model->GetInstalledServiceCount() == 0) {
    suggestions_label_->SetText(l10n_util::GetStringUTF16(
        IDS_INTENT_PICKER_GET_MORE_SERVICES_NONE_INSTALLED));
  } else {
    suggestions_label_->SetText(
        l10n_util::GetStringUTF16(IDS_INTENT_PICKER_GET_MORE_SERVICES));
  }

  service_buttons_->Update();
  extensions_->Update();
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnFaviconChanged(
    WebIntentPickerModel* model, size_t index) {
  service_buttons_->Update();
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const string16& extension_id) {
  extensions_->Update();
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnInlineDisposition(
    WebIntentPickerModel* model, const GURL& url) {
  if (!webview_)
    webview_ = new views::WebView(wrapper_->profile());

  inline_web_contents_.reset(WebContents::Create(
      wrapper_->profile(),
      tab_util::GetSiteInstanceForNewTab(wrapper_->profile(), url),
      MSG_ROUTING_NONE, NULL, NULL));
  // Does not take ownership, so we keep a scoped_ptr
  // for the WebContents locally.
  webview_->SetWebContents(inline_web_contents_.get());
  inline_disposition_delegate_.reset(
      new WebIntentInlineDispositionDelegate(this, inline_web_contents_.get(),
                                             wrapper_->profile()));
  content::WebContents* web_contents = webview_->GetWebContents();

  const WebIntentPickerModel::InstalledService* service =
      model->GetInstalledServiceWithURL(url);
  DCHECK(service);

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);
  web_contents->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());

  // Disable all buttons and show throbber.
  service_buttons_->SetEnabled(false);
  service_buttons_->StartThrobber(url);
  extensions_->SetEnabled(false);
  more_suggestions_link_->SetEnabled(false);
  contents_->Layout();
}

void WebIntentPickerViews::OnServiceButtonClicked(
    const WebIntentPickerModel::InstalledService& service) {
  delegate_->OnServiceChosen(service.url, service.disposition);
}

void WebIntentPickerViews::OnExtensionInstallClicked(
    const string16& extension_id) {
  service_buttons_->SetEnabled(false);
  extensions_->StartThrobber(extension_id);
  more_suggestions_link_->SetEnabled(false);
  contents_->Layout();
  delegate_->OnExtensionInstallRequested(UTF16ToUTF8(extension_id));
}

void WebIntentPickerViews::OnExtensionLinkClicked(
    const string16& extension_id) {
  delegate_->OnExtensionLinkClicked(UTF16ToUTF8(extension_id));
}

void WebIntentPickerViews::InitContents() {
  enum {
    kHeaderRowColumnSet,  // Column set for header layout.
    kFullWidthColumnSet,  // Column set with a single full-width column.
    kIndentedFullWidthColumnSet,  // Single full-width column, indented.
  };

  if (contents_) {
    // Replace the picker with the inline disposition.
    contents_->RemoveAllChildViews(true);
    displaying_web_contents_ = false;
  } else {
    contents_ = new views::View();
  }
  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(grid_layout);

  grid_layout->set_minimum_size(gfx::Size(kDialogMinWidth, 0));
  grid_layout->SetInsets(kContentAreaBorder, kContentAreaBorder,
                         kContentAreaBorder, kContentAreaBorder);
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(kHeaderRowColumnSet);
  header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Title.
  header_cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
#if defined(USE_CLOSE_BUTTON)
  header_cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                       GridLayout::USE_PREF, 0, 0);  // Close Button.
#endif

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

#if defined(USE_CLOSE_BUTTON)
  grid_layout->AddView(CreateCloseButton());
#endif

  // Padding row.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Service button row.
  grid_layout->StartRow(0, kFullWidthColumnSet);
  service_buttons_ = new ServiceButtonsView(model_, this);
  grid_layout->AddView(service_buttons_);

  // Row with app suggestions label.
  grid_layout->StartRow(0, kIndentedFullWidthColumnSet);
  suggestions_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_GET_MORE_SERVICES));
  suggestions_label_->SetMultiLine(true);
  suggestions_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  grid_layout->AddView(suggestions_label_);

  // Padding row.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Row with extension suggestions.
  grid_layout->StartRow(0, kIndentedFullWidthColumnSet);
  extensions_ = new SuggestedExtensionsView(model_, this);
  grid_layout->AddView(extensions_);

  // Padding row.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Row with "more suggestions" link.
  grid_layout->StartRow(0, kFullWidthColumnSet);
  more_suggestions_link_ = new views::Link(
    l10n_util::GetStringUTF16(IDS_INTENT_PICKER_MORE_SUGGESTIONS));
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
  InitContents();

  // Restore previous state.
  service_buttons_->Update();
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
  // TODO(binji): figure out how to get the constrained dialog centered...
  window_->SetSize(new_window_bounds.size());
}

#if defined(USE_CLOSE_BUTTON)
views::ImageButton* WebIntentPickerViews::CreateCloseButton() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  return close_button;
}
#endif
