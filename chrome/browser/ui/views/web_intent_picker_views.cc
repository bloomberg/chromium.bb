// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/browser.h"
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
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
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

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
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
  cs->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);
  cs->AddPaddingColumn(1, 0);

  for (size_t i = 0; i < model_->GetInstalledServiceCount(); ++i) {
    const WebIntentPickerModel::InstalledService& service =
        model_->GetInstalledServiceAt(i);

    grid_layout->StartRow(0, 0);

    views::NativeTextButton* button =
        new views::NativeTextButton(this, service.title);
    button->set_alignment(views::TextButton::ALIGN_LEFT);
    button->SetTooltipText(UTF8ToUTF16(service.url.spec().c_str()));
    button->SetIcon(*service.favicon.ToSkBitmap());
    button->set_tag(static_cast<int>(i));
    grid_layout->AddView(button);
    grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Additional space to separate the buttons from the suggestions.
  grid_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
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

  // A throbber to display when the extension is being installed.
  views::Throbber* throbber_;

  // The star rating of this extension.
  StarsView* stars_;

  // A button to install the extension.
  views::NativeTextButton* install_button_;

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

  throbber_ = new views::Throbber(60, true);
  throbber_->SetVisible(false);
  AddChildView(throbber_);

  stars_ = new StarsView(extension_->average_rating);
  AddChildView(stars_);

  install_button_= new views::NativeTextButton(
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
  stars_->SetVisible(false);
  install_button_->SetVisible(false);
  throbber_->SetVisible(true);
  throbber_->Start();
  Layout();
}

void SuggestedExtensionsRowView::StopThrobber() {
  stars_->SetVisible(true);
  install_button_->SetVisible(true);
  throbber_->SetVisible(false);
  throbber_->Stop();
  Layout();
}

void SuggestedExtensionsRowView::OnEnabledChanged() {
  title_link_->SetEnabled(enabled());
  stars_->SetVisible(enabled());
  install_button_->SetVisible(enabled());
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
// This view also displays the "More suggestions" link which searches the
// Chrome Web Store for more extensions.
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
  WebIntentPickerViews(Browser* browser,
                       TabContentsWrapper* tab_contents,
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
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE;

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
  ServiceButtonsView* service_buttons_;

  // A weak pointer to the header label for the extension suggestions.
  views::Label* suggestions_label_;

  // A weak pointer to the extensions view.
  SuggestedExtensionsView* extensions_;

  // Delegate for inline disposition tab contents.
  scoped_ptr<WebIntentInlineDispositionDelegate> inline_disposition_delegate_;

  // A weak pointer to the browser this picker is in.
  Browser* browser_;

  // A weak pointer to the view that contains all other views in the picker.
  views::View* contents_;

  // A weak pointer to the constrained window.
  ConstrainedWindowViews* window_;

  // A weak pointer to the more suggestions link.
  views::Link* more_suggestions_link_;

  // A weak pointer to the choose another service link.
  views::Link* choose_another_service_link_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerViews);
};

// static
WebIntentPicker* WebIntentPicker::Create(Browser* browser,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  WebIntentPickerViews* picker =
      new WebIntentPickerViews(browser, wrapper, delegate, model);

  return picker;
}

WebIntentPickerViews::WebIntentPickerViews(Browser* browser,
                                           TabContentsWrapper* wrapper,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
    : delegate_(delegate),
      model_(model),
      service_buttons_(NULL),
      suggestions_label_(NULL),
      extensions_(NULL),
      browser_(browser),
      contents_(NULL),
      window_(NULL),
      more_suggestions_link_(NULL),
      choose_another_service_link_(NULL) {
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
  delegate_->OnCancelled();
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
    // TODO(binji): This should link to a CWS search, based on the current
    // action/type pair.
    browser::NavigateParams params(
        browser_,
        GURL(extension_urls::GetWebstoreLaunchURL()),
        content::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&params);
  } else if (source == choose_another_service_link_) {
    // TODO(binji): Notify the controller that the user wants to pick again.
  } else {
    NOTREACHED();
  }
}

void WebIntentPickerViews::Close() {
  window_->CloseConstrainedWindow();
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
  WebContents* web_contents = WebContents::Create(
      browser_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_delegate_.reset(new WebIntentInlineDispositionDelegate);
  web_contents->SetDelegate(inline_disposition_delegate_.get());

  const WebIntentPickerModel::InstalledService* service =
      model->GetInstalledServiceWithURL(url);
  DCHECK(service);

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);

  TabContentsContainer* tab_contents_container = new TabContentsContainer;

  web_contents->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());

  // Replace the picker with the inline disposition.
  contents_->RemoveAllChildViews(true);

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

#if defined(USE_CLOSE_BUTTON)
  grid_layout->AddView(CreateCloseButton());
#endif

  // Inline web contents row.
  grid_layout->StartRow(0, 1);
  grid_layout->AddView(tab_contents_container, 1, 1, GridLayout::CENTER,
                       GridLayout::CENTER, kDialogMinWidth, 140);

  // The contents can only be changed after the child is added to view
  // hierarchy.
  tab_contents_container->ChangeWebContents(web_contents);

  contents_->Layout();
  SizeToContents();
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
  GURL extension_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                     UTF16ToUTF8(extension_id));
  browser::NavigateParams params(browser_,
                                 extension_url,
                                 content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
}

void WebIntentPickerViews::InitContents() {
  const int kHeaderRowColumnSet = 0;
  const int kFullWidthColumnSet = 1;
  const int kIndentedFullWidthColumnSet = 2;

  contents_ = new views::View();
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

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Header row.
  grid_layout->StartRow(0, kHeaderRowColumnSet);
  views::Label* top_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE));
  top_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  top_label->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  grid_layout->AddView(top_label);

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
  grid_layout->StartRow(0, kFullWidthColumnSet);
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
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
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
