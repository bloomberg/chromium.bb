// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
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

// The size, relative to default, of the Chrome Web store label.
const int kWebStoreLabelFontDelta = -2;

// A layout manager that asks for a given preferred size, and allocates all of
// its space to its only child.
class PreferredSizeFillLayout : public views::FillLayout {
 public:
  explicit PreferredSizeFillLayout(const gfx::Size& size) : size_(size) {}
  virtual ~PreferredSizeFillLayout() {}

  // LayoutManager implementation.
  virtual void Layout(views::View* host) OVERRIDE {
    views::FillLayout::Layout(host);
  }

  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE {
    DCHECK_EQ(1, host->child_count());
    return size_;
  }

 private:
  // The preferred size to allocate for the host of this layout manager.
  gfx::Size size_;
};

}  // namespace

// Views implementation of WebIntentPicker.
class WebIntentPickerViews : public views::ButtonListener,
                             public views::DialogDelegate,
                             public WebIntentPicker,
                             public WebIntentPickerModelObserver {
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
  virtual void OnInlineDisposition(WebIntentPickerModel* model) OVERRIDE;

 private:
  // Initialize the contents of the picker. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // Resize the constrained window to the size of its contents.
  void SizeToContents();

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // Web pointer to the picker model.
  WebIntentPickerModel* model_;

  // A weak pointer to the hbox that contains the buttons used to choose the
  // service.
  views::View* button_vbox_;

  // A vector of weak pointers to each of the service buttons.
  std::vector<views::TextButton*> buttons_;

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
      button_vbox_(NULL),
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
  std::vector<views::TextButton*>::iterator iter =
      std::find(buttons_.begin(), buttons_.end(), sender);
  DCHECK(iter != buttons_.end());

  size_t index = iter - buttons_.begin();
  const WebIntentPickerModel::Item& item = model_->GetItemAt(index);

  delegate_->OnServiceChosen(index, item.disposition);
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
  button_vbox_->RemoveAllChildViews(true);
  buttons_.clear();

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    const WebIntentPickerModel::Item& item = model_->GetItemAt(i);

    views::TextButton* button = new views::TextButton(this, item.title);
    button->SetTooltipText(UTF8ToUTF16(item.url.spec().c_str()));
    button->SetIcon(*item.favicon.ToSkBitmap());
    button_vbox_->AddChildView(button);

    buttons_.push_back(button);
  }

  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnFaviconChanged(WebIntentPickerModel* model,
                                            size_t index) {
  const WebIntentPickerModel::Item& item = model_->GetItemAt(index);
  views::TextButton* button = buttons_[index];
  button->SetIcon(*item.favicon.ToSkBitmap());
  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::OnInlineDisposition(
    WebIntentPickerModel* model) {
  const WebIntentPickerModel::Item& item =
      model->GetItemAt(model->inline_disposition_index());

  WebContents* web_contents = WebContents::Create(
      browser_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_delegate_.reset(new WebIntentInlineDispositionDelegate);
  web_contents->SetDelegate(inline_disposition_delegate_.get());

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);

  TabContentsContainer* tab_contents_container = new TabContentsContainer;

  web_contents->GetController().LoadURL(
      item.url,
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
  contents_->SetLayoutManager(new PreferredSizeFillLayout(size));

  contents_->Layout();
  SizeToContents();
}

void WebIntentPickerViews::InitContents() {
  contents_ = new views::View();
  contents_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical,
      0,  // inside border horizontal spacing
      0,  // inside border vertical spacing
      kContentAreaSpacing));  // between child spacing

  views::Label* top_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_CHOOSE_INTENT_HANDLER_MESSAGE));
  top_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  contents_->AddChildView(top_label);

  button_vbox_ = new views::View();
  button_vbox_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical,
      0,  // inside border horizontal spacing
      0,  // inside border vertical spacing
      kControlSpacing));  // between child spacing
  contents_->AddChildView(button_vbox_);

  views::Label* bottom_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE));
  bottom_label->SetMultiLine(true);
  bottom_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  bottom_label->SetFont(
      bottom_label->font().DeriveFont(kWebStoreLabelFontDelta));
  contents_->AddChildView(bottom_label);
}

void WebIntentPickerViews::SizeToContents() {
  gfx::Size client_size = contents_->GetPreferredSize();
  gfx::Rect client_bounds(client_size);
  gfx::Rect new_window_bounds = window_->non_client_view()->frame_view()->
      GetWindowBoundsForClientBounds(client_bounds);
  // TODO(binji): figure out how to get the constrained dialog centered...
  window_->SetSize(new_window_bounds.size());
}
