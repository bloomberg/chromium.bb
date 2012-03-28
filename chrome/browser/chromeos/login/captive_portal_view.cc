// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captive_portal_view.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info_bubble_view.h"
#include "chrome/browser/ui/views/reload_button.h"
#include "chrome/browser/ui/views/window.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using views::GridLayout;

namespace {

const char kCaptivePortalStartURL[] = "http://client3.google.com/generate_204";
const int kLocationBarHeight = 35;
// Margin between screen edge and CaptivePortalView border.
const int kExternalMargin = 50;
// Margin between DomView and CaptivePortalView border.
const int kInnerMargin = 2;

class ToolbarRowView : public views::View {
 public:
  ToolbarRowView() {
    set_background(views::Background::CreateSolidBackground(
        SkColorSetRGB(0xbe, 0xbe, 0xbe)));
  }

  virtual ~ToolbarRowView() {}

  void Init(views::View* back,
            views::View* forward,
            views::View* reload,
            views::View* location_bar) {
    GridLayout* layout = new GridLayout(this);
    SetLayoutManager(layout);

    // Back button.
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
    // Forward button.
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
    // Reload button.
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
    // Location bar.
    column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                          GridLayout::FIXED, kLocationBarHeight, 0);
    column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);

    layout->StartRow(0, 0);
    layout->AddView(back);
    layout->AddView(forward);
    layout->AddView(reload);
    layout->AddView(location_bar);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarRowView);
};

}  // namespace

namespace chromeos {

// Stub implementation of ContentSettingBubbleModelDelegate.
class StubBubbleModelDelegate : public ContentSettingBubbleModelDelegate {
 public:
  StubBubbleModelDelegate() {}
  virtual ~StubBubbleModelDelegate() {}

  // ContentSettingBubbleModelDelegate implementation:
  virtual void ShowCollectedCookiesDialog(
      TabContentsWrapper* contents) OVERRIDE {
  }

  virtual void ShowContentSettingsPage(ContentSettingsType type) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubBubbleModelDelegate);
};

// CaptivePortalView class -----------------------------------------------------

CaptivePortalView::CaptivePortalView(Profile* profile,
                                     CaptivePortalWindowProxy* proxy)
    : profile_(profile),
      redirected_(false),
      proxy_(proxy),
      back_(NULL),
      forward_(NULL),
      reload_(NULL),
      location_bar_(NULL),
      dom_view_(NULL),
      bubble_model_delegate_(new StubBubbleModelDelegate) {
  command_updater_.reset(new CommandUpdater(this));
  command_updater_->UpdateCommandEnabled(IDC_BACK, true);
  command_updater_->UpdateCommandEnabled(IDC_FORWARD, true);
  command_updater_->UpdateCommandEnabled(IDC_STOP, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE, true);
}

CaptivePortalView::~CaptivePortalView() {
  if (dom_view_container_.get()) {
    // DomView can't be deleted immediately, because it could be on the stack.
    dom_view_->dom_contents()->web_contents()->SetDelegate(NULL);
    MessageLoop::current()->DeleteSoon(
        FROM_HERE, dom_view_container_.release());
  }
}

void CaptivePortalView::StartLoad() {
  dom_view_container_.reset(new DOMView);
  dom_view_ = dom_view_container_.get();
  dom_view_->Init(profile_, NULL);
  dom_view_->dom_contents()->web_contents()->SetDelegate(this);
  dom_view_->LoadURL(GURL(kCaptivePortalStartURL));
}

void CaptivePortalView::Init() {
  set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Back/Forward buttons.
  back_ = new views::ImageButton(this);
  back_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                     ui::EF_MIDDLE_MOUSE_BUTTON);
  back_->set_tag(IDC_BACK);
  back_->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                           views::ImageButton::ALIGN_TOP);
  back_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_->set_id(VIEW_ID_BACK_BUTTON);

  forward_ = new views::ImageButton(this);
  forward_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                        ui::EF_MIDDLE_MOUSE_BUTTON);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward_->set_id(VIEW_ID_FORWARD_BUTTON);

  toolbar_model_.reset(new ToolbarModel(this));

  // Location bar.
  location_bar_ = new LocationBarView(profile_,
                                      command_updater_.get(),
                                      toolbar_model_.get(),
                                      this,
                                      LocationBarView::POPUP);

  // Reload button.
  reload_ = new ReloadButton(location_bar_, command_updater_.get());
  reload_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload_->set_id(VIEW_ID_RELOAD_BUTTON);

  LoadImages();

  // Use separate view to setup custom background.
  ToolbarRowView* toolbar_row = new ToolbarRowView;
  toolbar_row->Init(back_, forward_, reload_, location_bar_);

  // Layout.
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 0, 0);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, kInnerMargin);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 0, 0);
  column_set->AddPaddingColumn(0, kInnerMargin);

  // Setup layout rows.
  layout->StartRow(0, 0);
  layout->AddView(toolbar_row);

  layout->AddPaddingRow(0, kInnerMargin);

  layout->StartRow(1, 1);
  layout->AddView(dom_view_container_.release());
  layout->AddPaddingRow(0, kInnerMargin);

  location_bar_->Init();
  UpdateReload(dom_view_->dom_contents()->web_contents()->IsLoading(), true);

  Layout();
}

bool CaptivePortalView::CanResize() const {
  return false;
}

ui::ModalType CaptivePortalView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 CaptivePortalView::GetWindowTitle() const {
  string16 network_name;
  const Network* active_network =
      CrosLibrary::Get()->GetNetworkLibrary()->active_network();
  if (active_network) {
    network_name = ASCIIToUTF16(active_network->name());
  } else {
    DLOG(ERROR) << "No active network, but captive portal window is shown.";
  }

  return l10n_util::GetStringFUTF16(IDS_LOGIN_CAPTIVE_PORTAL_WINDOW_TITLE,
                                    network_name);
}

views::View* CaptivePortalView::GetContentsView() {
  return this;
}

views::View* CaptivePortalView::GetInitiallyFocusedView() {
  return dom_view_;
}

bool CaptivePortalView::ShouldShowWindowTitle() const {
  return true;
}

void CaptivePortalView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  command_updater_->ExecuteCommand(sender->tag());
}

void CaptivePortalView::NavigationStateChanged(
    const WebContents* source, unsigned changed_flags) {
  if (location_bar_) {
    location_bar_->Update(NULL);
    UpdateButtons();
  }

  // Naive way to determine the redirection. This won't be needed after portal
  // detection will be done on the Chrome side.
  GURL url = source->GetURL();
  // Note, |url| will be empty for "client3.google.com/generate_204" page.
  if (!redirected_  && url != GURL::EmptyGURL() &&
      url != GURL(kCaptivePortalStartURL)) {
    DLOG(INFO) << kCaptivePortalStartURL << " vs " << url.spec();
    redirected_ = true;
    proxy_->OnRedirected();
  }
}

void CaptivePortalView::LoadingStateChanged(WebContents* source) {
  bool is_loading = source->IsLoading();
  UpdateReload(is_loading, false);
  command_updater_->UpdateCommandEnabled(IDC_STOP, is_loading);
  // TODO(nkostylev): Fix case of no connectivity, check HTTP code returned.
  // Disable this heuristic as it has false positives.
  // Relying on just flimflam portal check to close dialog is fine.
  // if (!is_loading && !redirected_)
  //   proxy_->OnOriginalURLLoaded();
}

TabContentsWrapper* CaptivePortalView::GetTabContentsWrapper() const {
  return dom_view_->dom_contents();
}

InstantController* CaptivePortalView::GetInstant() {
  return NULL;
}

views::Widget* CaptivePortalView::CreateViewsBubble(
    views::BubbleDelegateView* bubble_delegate) {
  return browser::CreateViewsBubble(bubble_delegate);
}

ContentSettingBubbleModelDelegate*
    CaptivePortalView::GetContentSettingBubbleModelDelegate() {
  return bubble_model_delegate_.get();
}

void CaptivePortalView::ShowPageInfo(content::WebContents* web_contents,
                                     const GURL& url,
                                     const content::SSLStatus& ssl,
                                     bool show_history) {
    PageInfoBubbleView* page_info_bubble =
        new PageInfoBubbleView(
            location_bar_->location_icon_view(),
            location_bar_->profile(),
            url, ssl, true);
    page_info_bubble->set_parent_window(
        ash::Shell::GetInstance()->GetContainer(
            ash::internal::kShellWindowId_LockSystemModalContainer));
    CreateViewsBubble(page_info_bubble);
    page_info_bubble->Show();
}

PageActionImageView* CaptivePortalView::CreatePageActionImageView(
    LocationBarView* owner,
    ExtensionAction* action) {
  // Notreached because CaptivePortalView uses LocationBarView::POPUP type, and
  // it doesn't create PageActionImageViews.
  NOTREACHED();
  return NULL;
}

void CaptivePortalView::OnInputInProgress(bool in_progress) {
}

content::WebContents* CaptivePortalView::GetActiveWebContents() const {
  return dom_view_->dom_contents()->web_contents();
}

void CaptivePortalView::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition) {
  WebContents* web_contents = dom_view_->dom_contents()->web_contents();
  switch (id) {
    case IDC_BACK:
      if (web_contents->GetController().CanGoBack()) {
        location_bar_->Revert();
        web_contents->GetController().GoBack();
      }
      break;
    case IDC_FORWARD:
      if (web_contents->GetController().CanGoForward()) {
        location_bar_->Revert();
        web_contents->GetController().GoForward();
      }
      break;
    case IDC_STOP:
      web_contents->Stop();
      break;
    case IDC_RELOAD:
      // Always reload ignoring cache.
    case IDC_RELOAD_IGNORING_CACHE:
      web_contents->GetController().ReloadIgnoringCache(true);
      break;
    default:
      NOTREACHED();
  }
}

void CaptivePortalView::LoadImages() {
  ui::ThemeProvider* tp = GetThemeProvider();

  back_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_BACK));
  back_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_BACK_H));
  back_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_BACK_P));
  back_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_BACK_D));

  forward_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_FORWARD));
  forward_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_FORWARD_H));
  forward_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_FORWARD_P));
  forward_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_FORWARD_D));

  reload_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RELOAD_P));
  reload_->SetToggledImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_STOP));
  reload_->SetToggledImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_STOP_H));
  reload_->SetToggledImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STOP_P));
  reload_->SetToggledImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_STOP_D));
}

void CaptivePortalView::UpdateButtons() {
  const content::NavigationController& navigation_controller =
      dom_view_->dom_contents()->web_contents()->GetController();
  back_->SetEnabled(navigation_controller.CanGoBack());
  forward_->SetEnabled(navigation_controller.CanGoForward());
}

void CaptivePortalView::UpdateReload(bool is_loading, bool force) {
  if (reload_) {
    reload_->ChangeMode(
        is_loading ? ReloadButton::MODE_STOP : ReloadButton::MODE_RELOAD,
        force);
  }
}

}  // namespace chromeos
