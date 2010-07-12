// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/cookie_prompt_view.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/browser/views/cookie_info_view.h"
#include "chrome/browser/views/database_open_info_view.h"
#include "chrome/browser/views/generic_info_view.h"
#include "chrome/browser/views/local_storage_set_item_info_view.h"
#include "chrome/browser/views/options/content_settings_window_view.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/color_utils.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/non_client_view.h"

static const int kCookiePromptViewInsetSize = 5;

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, public:

CookiePromptView::CookiePromptView(
    CookiePromptModalDialog* parent,
    gfx::NativeWindow root_window,
    Profile* profile)
    : remember_radio_(NULL),
      ask_radio_(NULL),
      allow_button_(NULL),
      block_button_(NULL),
      show_cookie_link_(NULL),
      info_view_(NULL),
      session_expire_(false),
      expanded_view_(false),
      signaled_(false),
      parent_(parent),
      root_window_(root_window),
      profile_(profile) {
  InitializeViewResources();
  expanded_view_ = profile_->GetPrefs()->
      GetBoolean(prefs::kCookiePromptExpanded);
}

CookiePromptView::~CookiePromptView() {
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::View overrides:

gfx::Size CookiePromptView::GetPreferredSize() {
  gfx::Size client_size = views::View::GetPreferredSize();
  return gfx::Size(client_size.width(),
                   client_size.height() + GetExtendedViewHeight());
}


void CookiePromptView::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, ModalDialogDelegate implementation:

gfx::NativeWindow CookiePromptView::GetDialogRootWindow() {
  return root_window_;
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::DialogDelegate implementation:

std::wstring CookiePromptView::GetWindowTitle() const {
  return title_;
}

void CookiePromptView::WindowClosing() {
  if (!signaled_)
    parent_->BlockSiteData(false);
  parent_->CompleteDialog();
}

views::View* CookiePromptView::GetContentsView() {
  return this;
}

bool CookiePromptView::Accept() {
  parent_->AllowSiteData(remember_radio_->checked(), session_expire_);
  signaled_ = true;
  return true;
}

// CookieInfoViewDelegate overrides:
void CookiePromptView::ModifyExpireDate(bool session_expire) {
  session_expire_ = session_expire;
}


///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::ButtonListener implementation:

void CookiePromptView::ButtonPressed(views::Button* sender,
                                     const views::Event& event) {
  if (sender == allow_button_) {
    Accept();
    GetWindow()->Close();
  } else if (sender == block_button_) {
    parent_->BlockSiteData(remember_radio_->checked());
    signaled_ = true;
    GetWindow()->Close();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, views::LinkController implementation:
void CookiePromptView::LinkActivated(views::Link* source, int event_flags) {
  DCHECK_EQ(source, show_cookie_link_);
  ToggleDetailsViewExpand();
}

///////////////////////////////////////////////////////////////////////////////
// CookiePromptView, private:

void CookiePromptView::Init() {
  CookiePromptModalDialog::DialogType type = parent_->dialog_type();
  std::wstring display_host = UTF8ToWide(parent_->origin().host());
  views::Label* description_label = new views::Label(l10n_util::GetStringF(
      type == CookiePromptModalDialog::DIALOG_TYPE_COOKIE ?
          IDS_COOKIE_ALERT_LABEL : IDS_DATA_ALERT_LABEL,
      display_host));
  int radio_group_id = 0;
  remember_radio_ = new views::RadioButton(
      l10n_util::GetStringF(IDS_COOKIE_ALERT_REMEMBER_RADIO, display_host),
      radio_group_id);
  remember_radio_->set_listener(this);
  ask_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIE_ALERT_ASK_RADIO), radio_group_id);
  ask_radio_->set_listener(this);
  allow_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIE_ALERT_ALLOW_BUTTON));
  block_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIE_ALERT_BLOCK_BUTTON));
  show_cookie_link_ = new views::Link(
      l10n_util::GetString(IDS_COOKIE_SHOW_DETAILS_LABEL));
  show_cookie_link_->SetController(this);

  using views::GridLayout;

  GridLayout* layout = CreatePanelGridLayout(this);
  layout->SetInsets(kCookiePromptViewInsetSize, kCookiePromptViewInsetSize,
                    kCookiePromptViewInsetSize, kCookiePromptViewInsetSize);
  SetLayoutManager(layout);

  const int one_column_layout_id = 0;
  views::ColumnSet* one_column_set = layout->AddColumnSet(one_column_layout_id);
  one_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  one_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
  one_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(description_label);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(remember_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(ask_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  View* button_container = new View();
  GridLayout* button_layout = new GridLayout(button_container);
  button_container->SetLayoutManager(button_layout);
  const int inner_column_layout_id = 1;
  views::ColumnSet* inner_column_set = button_layout->AddColumnSet(
      inner_column_layout_id);
  inner_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                              GridLayout::USE_PREF, 0, 0);
  inner_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  inner_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                              GridLayout::USE_PREF, 0, 0);
  inner_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  inner_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                              GridLayout::USE_PREF, 0, 0);
  button_layout->StartRow(0, inner_column_layout_id);
  button_layout->AddView(show_cookie_link_, 1, 1,
                         GridLayout::LEADING, GridLayout::TRAILING);
  button_layout->AddView(allow_button_);
  button_layout->AddView(block_button_);

  int button_column_layout_id = 2;
  views::ColumnSet* button_column_set =
      layout->AddColumnSet(button_column_layout_id);
  button_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  button_column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                               GridLayout::USE_PREF, 0, 0);
  button_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  layout->StartRow(0, one_column_layout_id);
  layout->AddView(button_container, 1, 1,
                  GridLayout::FILL, GridLayout::CENTER);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, one_column_layout_id);

  if (type == CookiePromptModalDialog::DIALOG_TYPE_COOKIE) {
    CookieInfoView* cookie_info_view = new CookieInfoView(true);
    cookie_info_view->set_delegate(this);
    layout->AddView(cookie_info_view, 1, 1, GridLayout::FILL,
                    GridLayout::CENTER);

    cookie_info_view->SetCookieString(parent_->origin(),
                                      parent_->cookie_line());
    info_view_ = cookie_info_view;
  } else if (type == CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE) {
    LocalStorageSetItemInfoView* view = new LocalStorageSetItemInfoView();
    layout->AddView(view, 1, 1, GridLayout::FILL, GridLayout::CENTER);
    view->SetFields(parent_->origin().host(),
                    parent_->local_storage_key(),
                    parent_->local_storage_value());
    info_view_ = view;
  } else if (type == CookiePromptModalDialog::DIALOG_TYPE_DATABASE) {
    DatabaseOpenInfoView* view = new DatabaseOpenInfoView();
    layout->AddView(view, 1, 1, GridLayout::FILL, GridLayout::CENTER);
    view->SetFields(parent_->origin().host(),
                    parent_->database_name(),
                    parent_->display_name(),
                    parent_->estimated_size());
    info_view_ = view;
  } else if (type == CookiePromptModalDialog::DIALOG_TYPE_APPCACHE) {
    static const int kAppCacheInfoLabels[] = {
      IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL
    };
    GenericInfoView* view = new GenericInfoView(ARRAYSIZE(kAppCacheInfoLabels),
                                                kAppCacheInfoLabels);
    layout->AddView(view, 1, 1, GridLayout::FILL, GridLayout::CENTER);
    view->SetValue(0, UTF8ToUTF16(parent_->appcache_manifest_url().spec()));
    info_view_ = view;
  } else {
    NOTIMPLEMENTED();
  }

  info_view_->SetVisible(expanded_view_);

  // Set default values.
  remember_radio_->SetChecked(true);
}

int CookiePromptView::GetExtendedViewHeight() {
  DCHECK(info_view_);
  return expanded_view_ ?
      0 : -info_view_->GetPreferredSize().height();
}

void CookiePromptView::ToggleDetailsViewExpand() {
  int old_extended_height = GetExtendedViewHeight();

  expanded_view_ = !expanded_view_;
  profile_->GetPrefs()->SetBoolean(prefs::kCookiePromptExpanded,
                                   expanded_view_);

  // We have to set the visbility before asking for the extended view height
  // again as there is a bug in combobox that results in preferred height
  // changing when visible and not visible.
  info_view_->SetVisible(expanded_view_);
  int extended_height_delta = GetExtendedViewHeight() - old_extended_height;
  views::Window* window = GetWindow();
  gfx::Rect bounds = window->GetBounds();
  bounds.set_height(bounds.height() + extended_height_delta);
  window->SetBounds(bounds, NULL);
}

void CookiePromptView::InitializeViewResources() {
  CookiePromptModalDialog::DialogType type = parent_->dialog_type();
  title_ = l10n_util::GetStringF(
      type == CookiePromptModalDialog::DIALOG_TYPE_COOKIE ?
          IDS_COOKIE_ALERT_TITLE : IDS_DATA_ALERT_TITLE,
      UTF8ToWide(parent_->origin().host()));
}
