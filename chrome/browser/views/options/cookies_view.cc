// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/cookies_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time_format.h"
#include "chrome/browser/cookies_table_model.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "views/border.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/controls/button/native_button.h"
#include "views/controls/table/table_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"

// static
views::Window* CookiesView::instance_ = NULL;
static const int kCookieInfoViewBorderSize = 1;
static const int kCookieInfoViewInsetSize = 3;
static const int kSearchFilterDelayMs = 500;

///////////////////////////////////////////////////////////////////////////////
// CookiesTableView
//  Overridden to handle Delete key presses

class CookiesTableView : public views::TableView {
 public:
  CookiesTableView(CookiesTableModel* cookies_model,
                   std::vector<TableColumn> columns);
  virtual ~CookiesTableView() {}

  // Removes the cookies associated with the selected rows in the TableView.
  void RemoveSelectedCookies();

 private:
  // Our model, as a CookiesTableModel.
  CookiesTableModel* cookies_model_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTableView);
};

CookiesTableView::CookiesTableView(
    CookiesTableModel* cookies_model,
    std::vector<TableColumn> columns)
    : views::TableView(cookies_model, columns, views::ICON_AND_TEXT, false,
                       true, true),
      cookies_model_(cookies_model) {
}

void CookiesTableView::RemoveSelectedCookies() {
  // It's possible that we don't have anything selected.
  if (SelectedRowCount() <= 0)
    return;

  if (SelectedRowCount() == cookies_model_->RowCount()) {
    cookies_model_->RemoveAllShownCookies();
    return;
  }

  // Remove the selected cookies.  This iterates over the rows backwards, which
  // is required when calling RemoveCookies, see bug 2994.
  int last_selected_view_row = -1;
  int remove_count = 0;
  for (views::TableView::iterator i = SelectionBegin();
       i != SelectionEnd(); ++i) {
    int selected_model_row = *i;
    ++remove_count;
    if (last_selected_view_row == -1) {
      // Store the view row since the view to model mapping changes when
      // we delete.
      last_selected_view_row = model_to_view(selected_model_row);
    }
    cookies_model_->RemoveCookies(selected_model_row, 1);
  }

  // Select the next row after the last row deleted (unless removing last row).
  DCHECK(RowCount() > 0 && last_selected_view_row != -1);
  Select(view_to_model(std::min(RowCount() - 1,
      last_selected_view_row - remove_count + 1)));
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView
//
//  Responsible for displaying a tabular grid of Cookie information.
class CookieInfoView : public views::View {
 public:
  CookieInfoView();
  virtual ~CookieInfoView();

  // Update the display from the specified CookieNode.
  void SetCookie(const std::string& domain,
                 const net::CookieMonster::CanonicalCookie& cookie_node);

  // Clears the cookie display to indicate that no or multiple cookies are
  // selected.
  void ClearCookieDisplay();

  // Enables or disables the cookie proerty text fields.
  void EnableCookieDisplay(bool enabled);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // Set up the view layout
  void Init();

  // Individual property labels
  views::Label* name_label_;
  views::Textfield* name_value_field_;
  views::Label* content_label_;
  views::Textfield* content_value_field_;
  views::Label* domain_label_;
  views::Textfield* domain_value_field_;
  views::Label* path_label_;
  views::Textfield* path_value_field_;
  views::Label* send_for_label_;
  views::Textfield* send_for_value_field_;
  views::Label* created_label_;
  views::Textfield* created_value_field_;
  views::Label* expires_label_;
  views::Textfield* expires_value_field_;

  DISALLOW_COPY_AND_ASSIGN(CookieInfoView);
};

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
  cookies_table_->SetModel(NULL);
}

void CookiesView::UpdateSearchResults() {
  cookies_table_model_->UpdateSearchResults(search_field_->text());
  remove_all_button_->SetEnabled(cookies_table_model_->RowCount() > 0);
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::Buttonlistener implementation:

void CookiesView::ButtonPressed(views::Button* sender) {
  if (sender == remove_button_) {
    cookies_table_->RemoveSelectedCookies();
  } else if (sender == remove_all_button_) {
    // Delete all the Cookies shown.
    cookies_table_model_->RemoveAllShownCookies();
    UpdateForEmptyState();
  } else if (sender == clear_search_button_) {
    ResetSearchQuery();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::TableViewObserver implementation:
void CookiesView::OnSelectionChanged() {
  int selected_row_count = cookies_table_->SelectedRowCount();
  if (selected_row_count == 1) {
    int selected_index = cookies_table_->FirstSelectedRow();
    if (selected_index >= 0 &&
        selected_index < cookies_table_model_->RowCount()) {
      info_view_->SetCookie(cookies_table_model_->GetDomainAt(selected_index),
                            cookies_table_model_->GetCookieAt(selected_index));
    }
  } else {
    info_view_->ClearCookieDisplay();
  }
  remove_button_->SetEnabled(selected_row_count != 0);
  if (cookies_table_->RowCount() == 0)
    UpdateForEmptyState();
}

void CookiesView::OnTableViewDelete(views::TableView* table_view) {
  cookies_table_->RemoveSelectedCookies();
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

bool CookiesView::HandleKeystroke(views::Textfield* sender,
                                  const views::Textfield::Keystroke& key) {
  if (views::Textfield::IsKeystrokeEscape(key)) {
    ResetSearchQuery();
  } else if (views::Textfield::IsKeystrokeEnter(key)) {
    search_update_factory_.RevokeAll();
    UpdateSearchResults();
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesView, views::DialogDelegate implementation:

std::wstring CookiesView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_COOKIES_WINDOW_TITLE);
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
// CookiesView, private:

CookiesView::CookiesView(Profile* profile)
    : search_label_(NULL),
      search_field_(NULL),
      clear_search_button_(NULL),
      description_label_(NULL),
      cookies_table_(NULL),
      info_view_(NULL),
      remove_button_(NULL),
      remove_all_button_(NULL),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(search_update_factory_(this)) {
}

void CookiesView::Init() {
  search_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_SEARCH_LABEL));
  search_field_ = new views::Textfield;
  search_field_->SetController(this);
  clear_search_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_COOKIES_CLEAR_SEARCH_LABEL));
  clear_search_button_->SetEnabled(false);
  description_label_ = new views::Label(
      l10n_util::GetString(IDS_COOKIES_INFO_LABEL));
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  cookies_table_model_.reset(new CookiesTableModel(profile_));
  info_view_ = new CookieInfoView;
  std::vector<TableColumn> columns;
  columns.push_back(TableColumn(IDS_COOKIES_DOMAIN_COLUMN_HEADER,
                                TableColumn::LEFT, 200, 0.5f));
  columns.back().sortable = true;
  columns.push_back(TableColumn(IDS_COOKIES_NAME_COLUMN_HEADER,
                                TableColumn::LEFT, 150, 0.5f));
  columns.back().sortable = true;
  cookies_table_ = new CookiesTableView(cookies_table_model_.get(), columns);
  cookies_table_->SetObserver(this);
  // Make the table initially sorted by domain.
  views::TableView::SortDescriptors sort;
  sort.push_back(
      views::TableView::SortDescriptor(IDS_COOKIES_DOMAIN_COLUMN_HEADER,
                                       true));
  cookies_table_->SetSortDescriptors(sort);
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

  layout->StartRow(0, five_column_layout_id);
  layout->AddView(search_label_);
  layout->AddView(search_field_);
  layout->AddView(clear_search_button_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_layout_id);
  layout->AddView(description_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  layout->AddView(cookies_table_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_layout_id);
  layout->AddView(info_view_);

  // Add the Remove/Remove All buttons to the ClientView
  View* parent = GetParent();
  parent->AddChildView(remove_button_);
  parent->AddChildView(remove_all_button_);

  if (cookies_table_->RowCount() > 0) {
    cookies_table_->Select(0);
  } else {
    UpdateForEmptyState();
  }
}

void CookiesView::ResetSearchQuery() {
  search_field_->SetText(EmptyWString());
  clear_search_button_->SetEnabled(false);
  UpdateSearchResults();
}

void CookiesView::UpdateForEmptyState() {
  info_view_->ClearCookieDisplay();
  remove_button_->SetEnabled(false);
  remove_all_button_->SetEnabled(false);
}
