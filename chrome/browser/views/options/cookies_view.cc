// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/cookies_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "views/border.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/tree/tree_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"

// static
views::Window* CookiesView::instance_ = NULL;
static const int kCookieInfoViewBorderSize = 1;
static const int kCookieInfoViewInsetSize = 3;


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
  // Our model, as a CookiesTreeModel.
  CookiesTreeModel* cookies_model_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeView);
};

CookiesTreeView::CookiesTreeView(CookiesTreeModel* cookies_model)
    : cookies_model_(cookies_model) {
      SetModel(cookies_model_);
      SetRootShown(false);
      SetEditable(false);
}

void CookiesTreeView::RemoveSelectedItems() {
  TreeModelNode* selected_node = GetSelectedNode();
  if (selected_node) {
    cookies_model_->DeleteCookieNode(static_cast<CookieTreeCookieNode*>(
        GetSelectedNode()));
  }
}


///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, public:

CookieInfoView::CookieInfoView()
    : name_label_(NULL),
      name_value_field_(NULL),
      content_label_(NULL),
      content_value_field_(NULL),
      domain_label_(NULL),
      domain_value_field_(NULL),
      path_label_(NULL),
      path_value_field_(NULL),
      send_for_label_(NULL),
      send_for_value_field_(NULL),
      created_label_(NULL),
      created_value_field_(NULL),
      expires_label_(NULL),
      expires_value_field_(NULL) {
}

CookieInfoView::~CookieInfoView() {
}

void CookieInfoView::SetCookie(
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie) {
  name_value_field_->SetText(UTF8ToWide(cookie.Name()));
  content_value_field_->SetText(UTF8ToWide(cookie.Value()));
  domain_value_field_->SetText(UTF8ToWide(domain));
  path_value_field_->SetText(UTF8ToWide(cookie.Path()));
  created_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(cookie.CreationDate()));

  if (cookie.DoesExpire()) {
    expires_value_field_->SetText(
        base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate()));
  } else {
    // TODO(deanm) need a string that the average user can understand
    // "When you quit or restart your browser" ?
    expires_value_field_->SetText(
        l10n_util::GetString(IDS_COOKIES_COOKIE_EXPIRES_SESSION));
  }

  std::wstring sendfor_text;
  if (cookie.IsSecure()) {
    sendfor_text = l10n_util::GetString(IDS_COOKIES_COOKIE_SENDFOR_SECURE);
  } else {
    sendfor_text = l10n_util::GetString(IDS_COOKIES_COOKIE_SENDFOR_ANY);
  }
  send_for_value_field_->SetText(sendfor_text);
  EnableCookieDisplay(true);
}

void CookieInfoView::EnableCookieDisplay(bool enabled) {
  name_value_field_->SetEnabled(enabled);
  content_value_field_->SetEnabled(enabled);
  domain_value_field_->SetEnabled(enabled);
  path_value_field_->SetEnabled(enabled);
  send_for_value_field_->SetEnabled(enabled);
  created_value_field_->SetEnabled(enabled);
  expires_value_field_->SetEnabled(enabled);
}

void CookieInfoView::ClearCookieDisplay() {
  std::wstring no_cookie_string =
      l10n_util::GetString(IDS_COOKIES_COOKIE_NONESELECTED);
  name_value_field_->SetText(no_cookie_string);
  content_value_field_->SetText(no_cookie_string);
  domain_value_field_->SetText(no_cookie_string);
  path_value_field_->SetText(no_cookie_string);
  send_for_value_field_->SetText(no_cookie_string);
  created_value_field_->SetText(no_cookie_string);
  expires_value_field_->SetText(no_cookie_string);
  EnableCookieDisplay(false);
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, views::View overrides:

void CookieInfoView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, private:

void CookieInfoView::Init() {
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kCookieInfoViewBorderSize, border_color);
  set_border(border);

  name_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_NAME_LABEL));
  name_value_field_ = new views::Textfield;
  content_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_CONTENT_LABEL));
  content_value_field_ = new views::Textfield;
  domain_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_DOMAIN_LABEL));
  domain_value_field_ = new views::Textfield;
  path_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_PATH_LABEL));
  path_value_field_ = new views::Textfield;
  send_for_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_SENDFOR_LABEL));
  send_for_value_field_ = new views::Textfield;
  created_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_CREATED_LABEL));
  created_value_field_ = new views::Textfield;
  expires_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_EXPIRES_LABEL));
  expires_value_field_ = new views::Textfield;

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize);
  SetLayoutManager(layout);

  int three_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, three_column_layout_id);
  layout->AddView(name_label_);
  layout->AddView(name_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(content_label_);
  layout->AddView(content_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(domain_label_);
  layout->AddView(domain_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(path_label_);
  layout->AddView(path_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(send_for_label_);
  layout->AddView(send_for_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(created_label_);
  layout->AddView(created_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(expires_label_);
  layout->AddView(expires_value_field_);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  name_value_field_->SetReadOnly(true);
  name_value_field_->RemoveBorder();
  name_value_field_->SetBackgroundColor(text_area_background);
  content_value_field_->SetReadOnly(true);
  content_value_field_->RemoveBorder();
  content_value_field_->SetBackgroundColor(text_area_background);
  domain_value_field_->SetReadOnly(true);
  domain_value_field_->RemoveBorder();
  domain_value_field_->SetBackgroundColor(text_area_background);
  path_value_field_->SetReadOnly(true);
  path_value_field_->RemoveBorder();
  path_value_field_->SetBackgroundColor(text_area_background);
  send_for_value_field_->SetReadOnly(true);
  send_for_value_field_->RemoveBorder();
  send_for_value_field_->SetBackgroundColor(text_area_background);
  created_value_field_->SetReadOnly(true);
  created_value_field_->RemoveBorder();
  created_value_field_->SetBackgroundColor(text_area_background);
  expires_value_field_->SetReadOnly(true);
  expires_value_field_->RemoveBorder();
  expires_value_field_->SetBackgroundColor(text_area_background);
}

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
// CookiesView, views::Buttonlistener implementation:

void CookiesView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == remove_button_) {
    cookies_tree_->RemoveSelectedItems();
  } else if (sender == remove_all_button_) {
    cookies_tree_model_->DeleteAllCookies();
    UpdateForEmptyState();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::DialogDelegate implementation:

std::wstring CookiesView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE);
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
  CookieTreeNode::DetailedInfo detailed_info =
      static_cast<CookieTreeNode*>(tree_view->GetSelectedNode())->
      GetDetailedInfo();
  if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
    info_view_->SetCookie(detailed_info.cookie->first,
                          detailed_info.cookie->second);
  } else {
    info_view_->ClearCookieDisplay();
  }
}

void CookiesView::OnTreeViewKeyDown(base::KeyboardCode keycode) {
  if (keycode == base::VKEY_DELETE)
    cookies_tree_->RemoveSelectedItems();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, private:

CookiesView::CookiesView(Profile* profile)
    :
      description_label_(NULL),
      cookies_tree_(NULL),
      info_view_(NULL),
      remove_button_(NULL),
      remove_all_button_(NULL),
      profile_(profile) {
}

views::View* CookiesView::GetInitiallyFocusedView() {
  return cookies_tree_;
}

void CookiesView::Init() {
  description_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_INFO_LABEL));
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  cookies_tree_model_.reset(new CookiesTreeModel(profile_));
  info_view_ = new CookieInfoView;
  cookies_tree_ = new CookiesTreeView(cookies_tree_model_.get());
  remove_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIES_REMOVE_LABEL));
  remove_all_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIES_REMOVE_ALL_LABEL));

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
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

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(description_label_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  layout->AddView(cookies_tree_);
  cookies_tree_->ExpandAll();

  cookies_tree_->SetController(this);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_layout_id);
  layout->AddView(info_view_);

  // Add the Remove/Remove All buttons to the ClientView
  View* parent = GetParent();
  parent->AddChildView(remove_button_);
  parent->AddChildView(remove_all_button_);
  if (!cookies_tree_model_.get()->GetRoot()->GetChildCount())
    UpdateForEmptyState();
}

void CookiesView::UpdateForEmptyState() {
  info_view_->ClearCookieDisplay();
  remove_button_->SetEnabled(false);
  remove_all_button_->SetEnabled(false);
}
