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
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/image/image.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

using content::WebContents;

namespace {

// The space in pixels between the top-level groups in the dialog.
const int kContentAreaSpacing = 18;

// The space in pixels between the top-level groups and the dialog border.
const int kContentAreaBorder = 12;

// The space in pixels between controls in a group.
const int kControlSpacing = 6;

// The minimum size to display the constrained dialog.
const int kDialogMinWidth = 400;

// MinimumSizeView -------------------------------------------------------------

// A view with a minimum size.
class MinimumSizeView : public views::View {
 public:
  explicit MinimumSizeView(const gfx::Size& size);
  virtual ~MinimumSizeView();

  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(MinimumSizeView);
};

MinimumSizeView::MinimumSizeView(const gfx::Size& size)
  : size_(size) {
}

MinimumSizeView::~MinimumSizeView() {
}

gfx::Size MinimumSizeView::GetPreferredSize() {
  views::LayoutManager* layout_manager = GetLayoutManager();
  if (layout_manager) {
    gfx::Size lm_size = layout_manager->GetPreferredSize(this);
    return gfx::Size(std::max(lm_size.width(), size_.width()),
                     std::max(lm_size.height(), size_.height()));
  }

  return size_;
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
  if (model_->GetInstalledServiceCount() == 0)
    return;

  views::GridLayout* grid_layout = new views::GridLayout(this);
  SetLayoutManager(grid_layout);

  views::ColumnSet* cs = grid_layout->AddColumnSet(0);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
      views::GridLayout::USE_PREF, 0, 0);
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
  }

  // Add a padding row between the service buttons and the label when there
  // are service buttons to display.
  grid_layout->AddPaddingRow(0, kContentAreaSpacing);
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

// SuggestedExtensionsView -----------------------------------------------------

// A view that contains suggested extensions from the Chrome Web Store that
// provide an intent service matching the action/type pair.
// This view also displays the "More suggestions" link which searches the
// Chrome Web Store for more extensions.
class SuggestedExtensionsView : public views::View,
                                       views::ButtonListener,
                                       views::LinkListener {
 public:
  class Delegate {
   public:
    // Called when the user clicks the "Add to Chrome" button.
    virtual void OnExtensionInstallClicked(const string16& extension_id) = 0;

    // Called when the user clicks the extension title link.
    virtual void OnExtensionLinkClicked(const string16& extension_id) = 0;

    // Called when the user clicks the "More Suggestions" link.
    virtual void OnMoreSuggestionsLinkClicked() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // A helper class that forwards from the LinkListener to Delegate.
  class LinkListener : public views::LinkListener {
   public:
    LinkListener(Delegate* delegate, const string16& extension_id);
    virtual ~LinkListener();

    // LinkListener implementation.
    virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

   private:
    Delegate* delegate_;
    const string16& extension_id_;
  };

  SuggestedExtensionsView(const WebIntentPickerModel* model,
                          Delegate* delegate);

  virtual ~SuggestedExtensionsView();

  void Clear();

  // Update the view to the new model data.
  void Update();

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  typedef ScopedVector<LinkListener> LinkListeners;

  const WebIntentPickerModel* model_;
  Delegate* delegate_;
  LinkListeners link_listeners_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedExtensionsView);
};

SuggestedExtensionsView::LinkListener::LinkListener(
    Delegate* delegate,
    const string16& extension_id)
    : delegate_(delegate),
      extension_id_(extension_id) {
}

SuggestedExtensionsView::LinkListener::~LinkListener() {
}

void SuggestedExtensionsView::LinkListener::LinkClicked(views::Link* source,
                                                        int event_flags) {
  delegate_->OnExtensionLinkClicked(extension_id_);
}

SuggestedExtensionsView::SuggestedExtensionsView(
    const WebIntentPickerModel* model,
    Delegate* delegate)
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

  const int kNormalColumnSet = 0;
  const int kMoreSuggestionsColumnSet = 1;
  const views::GridLayout::Alignment kCenter = views::GridLayout::CENTER;
  const views::GridLayout::SizeType kUsePref = views::GridLayout::USE_PREF;

  views::GridLayout* grid_layout = new views::GridLayout(this);
  SetLayoutManager(grid_layout);
  views::ColumnSet* cs = grid_layout->AddColumnSet(kNormalColumnSet);
  cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Icon.
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Title.
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Stars.
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Install button.

  views::ColumnSet* more_cs = grid_layout->AddColumnSet(1);
  more_cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Icon.
  more_cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  more_cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Label.
  more_cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);

  if (model_->GetSuggestedExtensionCount() != 0) {
    for (size_t i = 0; i < model_->GetSuggestedExtensionCount(); ++i) {
      const WebIntentPickerModel::SuggestedExtension& extension =
          model_->GetSuggestedExtensionAt(i);

      grid_layout->StartRow(0, kNormalColumnSet);

      views::ImageView* icon = new views::ImageView();
      icon->SetImage(extension.icon.ToSkBitmap());
      grid_layout->AddView(icon);

      views::Link* link = new views::Link(extension.title);
      LinkListener* link_listener = new LinkListener(delegate_, extension.id);
      link->set_listener(link_listener);
      link_listeners_.push_back(link_listener);
      grid_layout->AddView(link);

      StarsView* stars = new StarsView(extension.average_rating);
      grid_layout->AddView(stars);

      views::NativeTextButton* install_button = new views::NativeTextButton(
          this,
          l10n_util::GetStringUTF16(IDS_INTENT_PICKER_INSTALL_EXTENSION));
      install_button->set_tag(static_cast<int>(i));
      grid_layout->AddView(install_button);
    }

    grid_layout->AddPaddingRow(0, kContentAreaSpacing);
  }

  // TODO(binji): Don't display more suggestions if there aren't any.

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  grid_layout->StartRow(0, kMoreSuggestionsColumnSet);
  views::ImageView* cws_icon = new views::ImageView();
  cws_icon->SetImage(rb.GetBitmapNamed(IDR_WEBSTORE_ICON_16));
  grid_layout->AddView(cws_icon);

  views::Link* cws_link = new views::Link(
    l10n_util::GetStringUTF16(IDS_INTENT_PICKER_MORE_SUGGESTIONS));
  cws_link->set_listener(this);
  grid_layout->AddView(cws_link);
}

void SuggestedExtensionsView::ButtonPressed(views::Button* sender,
                                            const views::Event& event) {
  size_t extension_index = static_cast<size_t>(sender->tag());
  const WebIntentPickerModel::SuggestedExtension& extension =
      model_->GetSuggestedExtensionAt(extension_index);

  delegate_->OnExtensionInstallClicked(extension.id);
}

void SuggestedExtensionsView::LinkClicked(views::Link* source,
                                          int event_flags) {
  delegate_->OnMoreSuggestionsLinkClicked();
}

}  // namespace

// WebIntentPickerViews --------------------------------------------------------

// Views implementation of WebIntentPicker.
class WebIntentPickerViews : public views::ButtonListener,
                             public views::DialogDelegate,
                             public WebIntentPicker,
                             public WebIntentPickerModelObserver,
                             public ServiceButtonsView::Delegate,
                             public SuggestedExtensionsView::Delegate {
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

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;

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

  // SuggestedExtensionsView::Delegate implementation.
  virtual void OnExtensionInstallClicked(const string16& extension_id) OVERRIDE;
  virtual void OnExtensionLinkClicked(const string16& extension_id) OVERRIDE;
  virtual void OnMoreSuggestionsLinkClicked() OVERRIDE;

 private:
  // Initialize the contents of the picker. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // Resize the constrained window to the size of its contents.
  void SizeToContents();

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
      window_(NULL) {
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
  delegate_->OnCancelled();
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

void WebIntentPickerViews::Close() {
  window_->CloseConstrainedWindow();
}

void WebIntentPickerViews::OnModelChanged(WebIntentPickerModel* model) {
  if (model->GetInstalledServiceCount() == 0) {
    suggestions_label_->SetText(l10n_util::GetStringUTF16(
        IDS_INTENT_PICKER_GET_MORE_SERVICES_NONE_INSTALLED));
  } else {
    suggestions_label_->SetText(
        l10n_util::GetStringUTF16(IDS_INTENT_PICKER_GET_MORE_SERVICES));
  }

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
  contents_->AddChildView(tab_contents_container);

  // The contents can only be changed after the child is added to view
  // hierarchy.
  tab_contents_container->ChangeWebContents(web_contents);

  gfx::Size size = GetDefaultInlineDispositionSize(web_contents);
  contents_->SetLayoutManager(new views::FillLayout);
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnServiceButtonClicked(
    const WebIntentPickerModel::InstalledService& service) {
  delegate_->OnServiceChosen(service.url, service.disposition);
}

void WebIntentPickerViews::OnExtensionInstallClicked(
    const string16& extension_id) {
  // TODO(binji): install extension.
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

void WebIntentPickerViews::OnMoreSuggestionsLinkClicked() {
  // TODO(binji): This should link to a CWS search, based on the current
  // action/type pair.
  browser::NavigateParams params(
      browser_,
      GURL(extension_urls::GetWebstoreLaunchURL()),
      content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
}

void WebIntentPickerViews::InitContents() {
  const int kHeaderRowColumnSet = 0;
  const int kFullWidthColumnSet = 1;
  const views::GridLayout::Alignment kCenter = views::GridLayout::CENTER;
  const views::GridLayout::Alignment kFill = views::GridLayout::FILL;
  const views::GridLayout::SizeType kUsePref = views::GridLayout::USE_PREF;

  contents_ = new MinimumSizeView(gfx::Size(kDialogMinWidth, 0));
  views::GridLayout* grid_layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(grid_layout);

  grid_layout->SetInsets(kContentAreaBorder, kContentAreaBorder,
                         kContentAreaBorder, kContentAreaBorder);
  views::ColumnSet* header_cs = grid_layout->AddColumnSet(kHeaderRowColumnSet);
  header_cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Title.
  header_cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  header_cs->AddColumn(kCenter, kCenter, 0, kUsePref, 0, 0);  // Close Button.

  views::ColumnSet* full_cs = grid_layout->AddColumnSet(kFullWidthColumnSet);
  full_cs->AddColumn(kFill, kCenter, 1, kUsePref, 0, 0);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Header row.
  grid_layout->StartRow(0, kHeaderRowColumnSet);
  views::Label* top_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE));
  top_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  top_label->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  grid_layout->AddView(top_label);

  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  grid_layout->AddView(close_button);

  // Padding row.
  grid_layout->AddPaddingRow(0, kContentAreaSpacing);

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
  grid_layout->AddPaddingRow(0, kContentAreaSpacing);

  // Row with extension suggestions.
  grid_layout->StartRow(0, kFullWidthColumnSet);
  extensions_ = new SuggestedExtensionsView(model_, this);
  grid_layout->AddView(extensions_);
}

void WebIntentPickerViews::SizeToContents() {
  gfx::Size client_size = contents_->GetPreferredSize();
  gfx::Rect client_bounds(client_size);
  gfx::Rect new_window_bounds = window_->non_client_view()->frame_view()->
      GetWindowBoundsForClientBounds(client_bounds);
  // TODO(binji): figure out how to get the constrained dialog centered...
  window_->SetSize(new_window_bounds.size());
}
