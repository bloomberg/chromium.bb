// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include <memory>

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

class ReadOnlyOriginView : public views::View {
 public:
  ReadOnlyOriginView(const base::string16& page_title,
                     const GURL& origin,
                     SkColor background_color) {
    SkColor foreground = GetForegroundColorForBackground(background_color);
    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>(this));

    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                       views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    std::unique_ptr<views::Label> title_label = std::make_unique<views::Label>(
        page_title, views::style::CONTEXT_DIALOG_TITLE);
    title_label->set_id(static_cast<int>(DialogViewID::SHEET_TITLE));
    title_label->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    // Turn off autoreadability because the computed |foreground| color takes
    // contrast into account.
    title_label->SetAutoColorReadabilityEnabled(false);
    title_label->SetEnabledColor(foreground);

    layout->AddView(title_label.release());

    layout->StartRow(0, 0);
    views::Label* origin_label =
        new views::Label(base::UTF8ToUTF16(origin.spec()));
    // Turn off autoreadability because the computed |foreground| color takes
    // contrast into account.
    origin_label->SetAutoColorReadabilityEnabled(false);
    origin_label->SetEnabledColor(foreground);

    origin_label->SetBackgroundColor(background_color);
    layout->AddView(origin_label);
  }
  ~ReadOnlyOriginView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadOnlyOriginView);
};

PaymentHandlerWebFlowViewController::PaymentHandlerWebFlowViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    Profile* profile,
    GURL target,
    PaymentHandlerOpenWindowCallback first_navigation_complete_callback)
    : PaymentRequestSheetController(spec, state, dialog),
      profile_(profile),
      target_(target),
      first_navigation_complete_callback_(
          std::move(first_navigation_complete_callback)) {}

PaymentHandlerWebFlowViewController::~PaymentHandlerWebFlowViewController() {}

base::string16 PaymentHandlerWebFlowViewController::GetSheetTitle() {
  if (web_contents())
    return web_contents()->GetTitle();

  return l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
}

void PaymentHandlerWebFlowViewController::FillContentView(
    views::View* content_view) {
  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile_);

  Observe(web_view->GetWebContents());
  web_view->LoadInitialURL(target_);

  // The webview must get an explicitly set height otherwise the layout doesn't
  // make it fill its container. This is likely because it has no content at the
  // time of first layout (nothing has loaded yet). Because of this, set it to.
  // total_dialog_height - header_height. On the other hand, the width will be
  // properly set so it can be 0 here.
  web_view->SetPreferredSize(gfx::Size(0, kDialogHeight - 68));
  content_view->AddChildView(web_view.release());
}

bool PaymentHandlerWebFlowViewController::ShouldShowSecondaryButton() {
  return false;
}

std::unique_ptr<views::View>
PaymentHandlerWebFlowViewController::CreateHeaderContentView() {
  const GURL origin = web_contents()
                          ? web_contents()->GetLastCommittedURL().GetOrigin()
                          : GURL();
  std::unique_ptr<views::Background> background = GetHeaderBackground();
  return std::make_unique<ReadOnlyOriginView>(GetSheetTitle(), origin,
                                              background->get_color());
}

std::unique_ptr<views::Background>
PaymentHandlerWebFlowViewController::GetHeaderBackground() {
  if (!web_contents())
    return PaymentRequestSheetController::GetHeaderBackground();
  return views::CreateSolidBackground(web_contents()->GetThemeColor());
}

void PaymentHandlerWebFlowViewController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (first_navigation_complete_callback_) {
    std::move(first_navigation_complete_callback_)
        .Run(true, web_contents()->GetMainFrame()->GetProcess()->GetID(),
             web_contents()->GetMainFrame()->GetRoutingID());
    first_navigation_complete_callback_ = PaymentHandlerOpenWindowCallback();
  }
  UpdateHeaderView();
}

void PaymentHandlerWebFlowViewController::TitleWasSet(
    content::NavigationEntry* entry) {
  UpdateHeaderView();
}

}  // namespace payments
