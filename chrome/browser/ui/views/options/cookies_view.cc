// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/cookies_view.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/appcache_info_view.h"
#include "chrome/browser/ui/views/cookie_info_view.h"
#include "chrome/browser/ui/views/database_info_view.h"
#include "chrome/browser/ui/views/indexed_db_info_view.h"
#include "chrome/browser/ui/views/local_storage_info_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/tree/tree_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

// static
views::Window* CookiesView::instance_ = NULL;
static const int kSearchFilterDelayMs = 500;

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeView
//  Overridden to handle Delete key presses

class CookiesTreeView : public views::TreeView {
 public:
  explicit CookiesTreeView(CookiesTreeModel* cookies_model);
  virtual ~CookiesTreeView() {}

  // Removes the items associated with the selected node in the TreeView
  void RemoveSelectedItems();

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiesTreeView);
};

CookiesTreeView::CookiesTreeView(CookiesTreeModel* cookies_model) {
  SetModel(cookies_model);
  SetRootShown(false);
  SetEditable(false);
}

void CookiesTreeView::RemoveSelectedItems() {
  ui::TreeModelNode* selected_node = GetSelectedNode();
  if (selected_node) {
    static_cast<CookiesTreeModel*>(model())->DeleteCookieNode(
        static_cast<CookieTreeNode*>(GetSelectedNode()));
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView::InfoPanelView
//  Overridden to handle layout of the various info views.
//
// This view is a child of the CookiesView and participates
// in its GridLayout. The various info views are all children
// of this view. Only one child is expected to be visible at a time.

class CookiesView::InfoPanelView : public views::View {
 public:
  virtual void Layout() {
    int child_count = GetChildViewCount();
    for (int i = 0; i < child_count; ++i)
      GetChildViewAt(i)->SetBounds(0, 0, width(), height());
  }

  virtual gfx::Size GetPreferredSize() {
    DCHECK(GetChildViewCount() > 0);
    return GetChildViewAt(0)->GetPreferredSize();
  }
};

///////////////////////////////////////////////////////////////////////////////
// CookiesView, public:

// static
void CookiesView::ShowCookiesWindow(Profile* profile) {
  if (!instance_) {
    CookiesView* cookies_view = new CookiesView(profile);
    instance_ = views::Window::CreateChromeWindow(
        NULL, gfx::Rect(), cookies_view);
  }
  if (!instance_->IsVisible()) {
    instance_->Show();
  } else {
    instance_->Activate();
  }
}

CookiesView::~CookiesView() {
  cookies_tree_->SetModel(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, TreeModelObserver overrides:

void CookiesView::TreeNodesAdded(ui::TreeModel* model,
                                 ui::TreeModelNode* parent,
                                 int start,
                                 int count) {
  UpdateRemoveButtonsState();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::Buttonlistener implementation:

void CookiesView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == remove_button_) {
    cookies_tree_->RemoveSelectedItems();
    if (cookies_tree_model_->GetRoot()->GetChildCount() == 0)
      UpdateForEmptyState();
  } else if (sender == remove_all_button_) {
    cookies_tree_model_->DeleteAllStoredObjects();
    UpdateForEmptyState();
  } else if (sender == clear_search_button_) {
    ResetSearchQuery();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::Textfield::Controller implementation:

void CookiesView::ContentsChanged(views::Textfield* sender,
                                  const std::wstring& new_contents) {
  clear_search_button_->SetEnabled(!search_field_->text().empty());
  search_update_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      search_update_factory_.NewRunnableMethod(
          &CookiesView::UpdateSearchResults), kSearchFilterDelayMs);
}

bool CookiesView::HandleKeyEvent(views::Textfield* sender,
                                 const views::KeyEvent& key_event) {
  if (key_event.GetKeyCode() == ui::VKEY_ESCAPE) {
    ResetSearchQuery();
  } else if (key_event.GetKeyCode() == ui::VKEY_RETURN) {
    search_update_factory_.RevokeAll();
    UpdateSearchResults();
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::DialogDelegate implementation:

std::wstring CookiesView::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE));
}

void CookiesView::WindowClosing() {
  instance_ = NULL;
}

views::View* CookiesView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::View overrides:

void CookiesView::Layout() {
  // Lay out the Remove/Remove All buttons in the parent view.
  gfx::Size ps = remove_button_->GetPreferredSize();
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);
  int y_buttons = parent_bounds.bottom() - ps.height() - kButtonVEdgeMargin;

  remove_button_->SetBounds(kPanelHorizMargin, y_buttons, ps.width(),
                            ps.height());

  ps = remove_all_button_->GetPreferredSize();
  int remove_all_x = remove_button_->x() + remove_button_->width() +
      kRelatedControlHorizontalSpacing;
  remove_all_button_->SetBounds(remove_all_x, y_buttons, ps.width(),
                                ps.height());

  // Lay out this View
  View::Layout();
}

gfx::Size CookiesView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_COOKIES_DIALOG_WIDTH_CHARS,
      IDS_COOKIES_DIALOG_HEIGHT_LINES));
}

void CookiesView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::TreeViewController overrides:

void CookiesView::OnTreeViewSelectionChanged(views::TreeView* tree_view) {
  UpdateRemoveButtonsState();
  CookieTreeNode::DetailedInfo detailed_info =
      static_cast<CookieTreeNode*>(tree_view->GetSelectedNode())->
      GetDetailedInfo();
  if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
    UpdateVisibleDetailedInfo(cookie_info_view_);
    cookie_info_view_->SetCookie(detailed_info.cookie->Domain(),
                                 *detailed_info.cookie);
  } else if (detailed_info.node_type ==
             CookieTreeNode::DetailedInfo::TYPE_DATABASE) {
    UpdateVisibleDetailedInfo(database_info_view_);
    database_info_view_->SetDatabaseInfo(*detailed_info.database_info);
  } else if (detailed_info.node_type ==
             CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE) {
    UpdateVisibleDetailedInfo(local_storage_info_view_);
    local_storage_info_view_->SetLocalStorageInfo(
        *detailed_info.local_storage_info);
  } else if (detailed_info.node_type ==
             CookieTreeNode::DetailedInfo::TYPE_APPCACHE) {
    UpdateVisibleDetailedInfo(appcache_info_view_);
    appcache_info_view_->SetAppCacheInfo(detailed_info.appcache_info);
  } else if (detailed_info.node_type ==
             CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB) {
    UpdateVisibleDetailedInfo(indexed_db_info_view_);
    indexed_db_info_view_->SetIndexedDBInfo(*detailed_info.indexed_db_info);
  } else {
    UpdateVisibleDetailedInfo(cookie_info_view_);
    cookie_info_view_->ClearCookieDisplay();
  }
}

void CookiesView::OnTreeViewKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_DELETE)
    cookies_tree_->RemoveSelectedItems();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, public:

void CookiesView::UpdateSearchResults() {
  cookies_tree_model_->UpdateSearchResults(search_field_->text());
  UpdateRemoveButtonsState();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, private:

CookiesView::CookiesView(Profile* profile)
    :
      search_label_(NULL),
      search_field_(NULL),
      clear_search_button_(NULL),
      description_label_(NULL),
      cookies_tree_(NULL),
      info_panel_(NULL),
      cookie_info_view_(NULL),
      database_info_view_(NULL),
      local_storage_info_view_(NULL),
      appcache_info_view_(NULL),
      indexed_db_info_view_(NULL),
      remove_button_(NULL),
      remove_all_button_(NULL),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(search_update_factory_(this)) {
}

void CookiesView::Init() {
  search_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_SEARCH_LABEL)));
  search_field_ = new views::Textfield;
  search_field_->SetController(this);
  clear_search_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_CLEAR_SEARCH_LABEL)));
  clear_search_button_->SetEnabled(false);
  description_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_INFO_LABEL)));
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  cookies_tree_model_.reset(new CookiesTreeModel(
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster(),
      new BrowsingDataDatabaseHelper(profile_),
      new BrowsingDataLocalStorageHelper(profile_),
      NULL,
      new BrowsingDataAppCacheHelper(profile_),
      BrowsingDataIndexedDBHelper::Create(profile_)));
  cookies_tree_model_->AddObserver(this);

  info_panel_ = new InfoPanelView;
  cookie_info_view_ = new CookieInfoView(false);
  database_info_view_ = new DatabaseInfoView;
  local_storage_info_view_ = new LocalStorageInfoView;
  appcache_info_view_ = new AppCacheInfoView;
  indexed_db_info_view_ = new IndexedDBInfoView;
  info_panel_->AddChildView(cookie_info_view_);
  info_panel_->AddChildView(database_info_view_);
  info_panel_->AddChildView(local_storage_info_view_);
  info_panel_->AddChildView(appcache_info_view_);
  info_panel_->AddChildView(indexed_db_info_view_);

  cookies_tree_ = new CookiesTreeView(cookies_tree_model_.get());
  remove_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_REMOVE_LABEL)));
  remove_all_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_REMOVE_ALL_LABEL)));

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int five_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(five_column_layout_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  const int single_column_layout_id = 1;
  column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, five_column_layout_id);
  layout->AddView(search_label_);
  layout->AddView(search_field_);
  layout->AddView(clear_search_button_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(description_label_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  cookies_tree_->set_lines_at_root(true);
  cookies_tree_->set_auto_expand_children(true);
  layout->AddView(cookies_tree_);

  cookies_tree_->SetController(this);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_layout_id);
  layout->AddView(info_panel_);

  // Add the Remove/Remove All buttons to the ClientView
  View* parent = GetParent();
  parent->AddChildView(remove_button_);
  parent->AddChildView(remove_all_button_);
  if (!cookies_tree_model_.get()->GetRoot()->GetChildCount()) {
    UpdateForEmptyState();
  } else {
    UpdateVisibleDetailedInfo(cookie_info_view_);
    UpdateRemoveButtonsState();
  }
}

void CookiesView::ResetSearchQuery() {
  search_field_->SetText(std::wstring());
  clear_search_button_->SetEnabled(false);
  UpdateSearchResults();
}

void CookiesView::UpdateForEmptyState() {
  cookie_info_view_->ClearCookieDisplay();
  remove_button_->SetEnabled(false);
  remove_all_button_->SetEnabled(false);
  UpdateVisibleDetailedInfo(cookie_info_view_);
}

void CookiesView::UpdateRemoveButtonsState() {
  remove_button_->SetEnabled(cookies_tree_model_->GetRoot()->
      GetTotalNodeCount() > 1 && cookies_tree_->GetSelectedNode());
  remove_all_button_->SetEnabled(cookies_tree_model_->GetRoot()->
      GetTotalNodeCount() > 1);
}

void CookiesView::UpdateVisibleDetailedInfo(views::View* view) {
  cookie_info_view_->SetVisible(view == cookie_info_view_);
  database_info_view_->SetVisible(view == database_info_view_);
  local_storage_info_view_->SetVisible(view == local_storage_info_view_);
  appcache_info_view_->SetVisible(view == appcache_info_view_);
  indexed_db_info_view_->SetVisible(view == indexed_db_info_view_);
}
