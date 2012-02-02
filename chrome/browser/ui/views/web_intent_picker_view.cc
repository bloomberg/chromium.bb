// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/window.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

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

}  // namespace

// Views implementation of WebIntentPicker.
class WebIntentPickerView : public views::BubbleDelegateView,
                            public views::ButtonListener,
                            public WebIntentPicker,
                            public WebIntentPickerModelObserver {
 public:
  WebIntentPickerView(views::View* anchor_view,
                      TabContentsWrapper* tab_contents,
                      WebIntentPickerDelegate* delegate,
                      WebIntentPickerModel* model);
  virtual ~WebIntentPickerView();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::WidgetDelegate implementation.
  virtual void WindowClosing() OVERRIDE;

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnInlineDisposition(WebIntentPickerModel* model) OVERRIDE;

 protected:
  // views::BubbleDelegateView overrides:
  void Init() OVERRIDE;

 private:
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

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerView);
};

// static
WebIntentPicker* WebIntentPicker::Create(Browser* browser,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  // Find where to point the bubble at.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  WebIntentPickerView* bubble_delegate =
      new WebIntentPickerView(anchor_view, wrapper, delegate, model);
  browser::CreateViewsBubble(bubble_delegate);
  bubble_delegate->Show();
  return bubble_delegate;
}

WebIntentPickerView::WebIntentPickerView(views::View* anchor_view,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      delegate_(delegate),
      model_(model),
      button_vbox_(NULL) {
  model_->set_observer(this);
}

WebIntentPickerView::~WebIntentPickerView() {
  model_->set_observer(NULL);
}

void WebIntentPickerView::ButtonPressed(views::Button* sender,
                                        const views::Event& event) {
  std::vector<views::TextButton*>::iterator iter =
      std::find(buttons_.begin(), buttons_.end(), sender);
  DCHECK(iter != buttons_.end());

  size_t index = iter - buttons_.begin();
  const WebIntentPickerModel::Item& item = model_->GetItemAt(index);

  delegate_->OnServiceChosen(index, item.disposition);
}

void WebIntentPickerView::WindowClosing() {
  delegate_->OnClosing();
}

void WebIntentPickerView::Close() {
  GetWidget()->Close();
}

void WebIntentPickerView::OnModelChanged(WebIntentPickerModel* model) {
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
}

void WebIntentPickerView::OnFaviconChanged(WebIntentPickerModel* model,
                                           size_t index) {
  const WebIntentPickerModel::Item& item = model_->GetItemAt(index);
  views::TextButton* button = buttons_[index];

  button->SetIcon(*item.favicon.ToSkBitmap());

  Layout();
  SizeToContents();
}

void WebIntentPickerView::OnInlineDisposition(WebIntentPickerModel* model) {
  // TODO(gbillock): add support here.
  delegate_->OnInlineDispositionWebContentsCreated(NULL);
}

void WebIntentPickerView::Init() {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal,
      kContentAreaBorder,  // inside border horizontal spacing
      kContentAreaBorder,  // inside border vertical spacing
      kContentAreaSpacing));  // between child spacing

  views::View* main_content = new views::View();
  main_content->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical,
      0,  // inside border horizontal spacing
      0,  // inside border vertical spacing
      kContentAreaSpacing));  // between child spacing

  views::Label* top_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_CHOOSE_INTENT_HANDLER_MESSAGE));
  top_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  main_content->AddChildView(top_label);

  button_vbox_ = new views::View();
  button_vbox_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical,
      0,  // inside border horizontal spacing
      0,  // inside border vertical spacing
      kControlSpacing));  // between child spacing
  main_content->AddChildView(button_vbox_);

  views::Label* bottom_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE));
  bottom_label->SetMultiLine(true);
  bottom_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  bottom_label->SetFont(
      bottom_label->font().DeriveFont(kWebStoreLabelFontDelta));
  main_content->AddChildView(bottom_label);

  AddChildView(main_content);
}
