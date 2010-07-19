// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/collected_cookies_win.h"

#include "app/l10n_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowCollectedCookiesDialog(gfx::NativeWindow parent_window,
                                TabContents* tab_contents) {
  // Deletes itself on close.
  new CollectedCookiesWin(parent_window, tab_contents);
}

} // namespace browser


///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesWin, constructor and destructor:

CollectedCookiesWin::CollectedCookiesWin(gfx::NativeWindow parent_window,
                                         TabContents* tab_contents)
    : tab_contents_(tab_contents),
      allowed_label_(NULL),
      blocked_label_(NULL),
      allowed_cookies_tree_(NULL),
      blocked_cookies_tree_(NULL),
      block_allowed_button_(NULL),
      allow_blocked_button_(NULL),
      for_session_blocked_button_(NULL) {
  TabSpecificContentSettings* content_settings =
      tab_contents->GetTabSpecificContentSettings();
  registrar_.Add(this, NotificationType::COLLECTED_COOKIES_SHOWN,
                 Source<TabSpecificContentSettings>(content_settings));

  Init();

  window_ = tab_contents_->CreateConstrainedDialog(this);
}

CollectedCookiesWin::~CollectedCookiesWin() {
  allowed_cookies_tree_->SetModel(NULL);
  blocked_cookies_tree_->SetModel(NULL);
}

void CollectedCookiesWin::Init() {
  TabSpecificContentSettings* content_settings =
      tab_contents_->GetTabSpecificContentSettings();

  // Allowed Cookie list.
  allowed_label_ = new views::Label(
      l10n_util::GetString(IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL));
  allowed_cookies_tree_model_.reset(
      content_settings->GetAllowedCookiesTreeModel());
  allowed_cookies_tree_ = new views::TreeView();
  allowed_cookies_tree_->SetModel(allowed_cookies_tree_model_.get());
  allowed_cookies_tree_->SetController(this);
  allowed_cookies_tree_->SetRootShown(false);
  allowed_cookies_tree_->SetEditable(false);
  allowed_cookies_tree_->set_lines_at_root(true);
  allowed_cookies_tree_->set_auto_expand_children(true);

  // Blocked Cookie list.
  blocked_label_ = new views::Label(
      l10n_util::GetString(IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL));
  blocked_cookies_tree_model_.reset(
      content_settings->GetBlockedCookiesTreeModel());
  blocked_cookies_tree_ = new views::TreeView();
  blocked_cookies_tree_->SetModel(blocked_cookies_tree_model_.get());
  blocked_cookies_tree_->SetController(this);
  blocked_cookies_tree_->SetRootShown(false);
  blocked_cookies_tree_->SetEditable(false);
  blocked_cookies_tree_->set_lines_at_root(true);
  blocked_cookies_tree_->set_auto_expand_children(true);

  using views::GridLayout;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int three_columns_layout_id = 1;
  column_set = layout->AddColumnSet(three_columns_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(allowed_label_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  layout->AddView(
      allowed_cookies_tree_, 1, 1, GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  block_allowed_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COLLECTED_COOKIES_BLOCK_BUTTON));
  layout->AddView(
      block_allowed_button_, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(blocked_label_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  layout->AddView(
      blocked_cookies_tree_, 1, 1, GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, three_columns_layout_id);
  allow_blocked_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COLLECTED_COOKIES_ALLOW_BUTTON));
  layout->AddView(allow_blocked_button_);
  for_session_blocked_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON));
  layout->AddView(for_session_blocked_button_);

  EnableControls();
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedDialogDelegate implementation.

std::wstring CollectedCookiesWin::GetWindowTitle() const {
  return l10n_util::GetString(IDS_COLLECTED_COOKIES_DIALOG_TITLE);
}

int CollectedCookiesWin::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring CollectedCookiesWin::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  return l10n_util::GetString(IDS_CLOSE);
}

void CollectedCookiesWin::DeleteDelegate() {
  delete this;
}

bool CollectedCookiesWin::Cancel() {
  return true;
}

views::View* CollectedCookiesWin::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation.

void CollectedCookiesWin::ButtonPressed(views::Button* sender,
                                        const views::Event& event) {
  if (sender == block_allowed_button_)
    AddContentException(allowed_cookies_tree_, CONTENT_SETTING_BLOCK);
  else if (sender == allow_blocked_button_)
    AddContentException(blocked_cookies_tree_, CONTENT_SETTING_ALLOW);
  else if (sender == for_session_blocked_button_)
    AddContentException(blocked_cookies_tree_, CONTENT_SETTING_SESSION_ONLY);
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementation.

void CollectedCookiesWin::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
  EnableControls();
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementation.

gfx::Size CollectedCookiesWin::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_COOKIES_DIALOG_WIDTH_CHARS,
      IDS_COOKIES_DIALOG_HEIGHT_LINES));
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesWin, private methods.

void CollectedCookiesWin::EnableControls() {
  bool enable_allowed_buttons = false;
  TreeModelNode* node = allowed_cookies_tree_->GetSelectedNode();
  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_ORIGIN) {
      enable_allowed_buttons = static_cast<CookieTreeOriginNode*>(
          cookie_node)->CanCreateContentException();
    }
  }
  block_allowed_button_->SetEnabled(enable_allowed_buttons);

  bool enable_blocked_buttons = false;
  node = blocked_cookies_tree_->GetSelectedNode();
  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_ORIGIN) {
      enable_blocked_buttons = static_cast<CookieTreeOriginNode*>(
          cookie_node)->CanCreateContentException();
    }
  }
  allow_blocked_button_->SetEnabled(enable_blocked_buttons);
  for_session_blocked_button_->SetEnabled(enable_blocked_buttons);
}

void CollectedCookiesWin::AddContentException(views::TreeView* tree_view,
                                              ContentSetting setting) {
  CookieTreeOriginNode* origin_node =
      static_cast<CookieTreeOriginNode*>(tree_view->GetSelectedNode());
  origin_node->CreateContentException(
      tab_contents_->profile()->GetHostContentSettingsMap(), setting);
}

///////////////////////////////////////////////////////////////////////////////
// NotificationObserver implementation.

void CollectedCookiesWin::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(type == NotificationType::COLLECTED_COOKIES_SHOWN);
  DCHECK_EQ(Source<TabSpecificContentSettings>(source).ptr(),
            tab_contents_->GetTabSpecificContentSettings());
  window_->CloseConstrainedWindow();
}
