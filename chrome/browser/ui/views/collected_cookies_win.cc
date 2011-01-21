// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/collected_cookies_win.h"

#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "gfx/color_utils.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/box_layout.h"
#include "views/controls/button/native_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/standard_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowCollectedCookiesDialog(gfx::NativeWindow parent_window,
                                TabContents* tab_contents) {
  // Deletes itself on close.
  new CollectedCookiesWin(parent_window, tab_contents);
}

}  // namespace browser

namespace {
// Spacing between the infobar frame and its contents.
const int kInfobarVerticalPadding = 3;
const int kInfobarHorizontalPadding = 8;

// Width of the infobar frame.
const int kInfobarBorderSize = 1;

// Dimensions of the tree views.
const int kTreeViewWidth = 400;
const int kTreeViewHeight = 125;

}  // namespace

// A custom view that conditionally displays an infobar.
class InfobarView : public views::View {
 public:
  InfobarView() {
    content_ = new views::View;
    SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
    views::Border* border = views::Border::CreateSolidBorder(
        kInfobarBorderSize, border_color);
    content_->set_border(border);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    info_image_ = new views::ImageView();
    info_image_->SetImage(rb.GetBitmapNamed(IDR_INFO));
    label_ = new views::Label();
  }
  virtual ~InfobarView() {}

  // Update the visibility of the infobar. If |is_visible| is true, a rule for
  // |setting| on |domain_name| was created.
  void UpdateVisibility(bool is_visible,
                        ContentSetting setting,
                        const std::wstring& domain_name) {
    if (!is_visible) {
      SetVisible(false);
      return;
    }

    std::wstring label;
    switch (setting) {
      case CONTENT_SETTING_BLOCK:
        label = UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_BLOCK_RULE_CREATED,
            WideToUTF16(domain_name)));
        break;

      case CONTENT_SETTING_ALLOW:
        label = UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_ALLOW_RULE_CREATED,
            WideToUTF16(domain_name)));
        break;

      case CONTENT_SETTING_SESSION_ONLY:
        label = UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_SESSION_RULE_CREATED,
            WideToUTF16(domain_name)));
        break;

      default:
        NOTREACHED();
    }
    label_->SetText(label);
    content_->Layout();
    SetVisible(true);
  }

 private:
  // Initialize contents and layout.
  void Init() {
    AddChildView(content_);
    content_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             kInfobarHorizontalPadding,
                             kInfobarVerticalPadding,
                             kRelatedControlSmallHorizontalSpacing));
    content_->AddChildView(info_image_);
    content_->AddChildView(label_);
    UpdateVisibility(false, CONTENT_SETTING_BLOCK, std::wstring());
  }

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() {
    if (!IsVisible())
      return gfx::Size();

    // Add space around the banner.
    gfx::Size size(content_->GetPreferredSize());
    size.Enlarge(0, 2 * kRelatedControlVerticalSpacing);
    return size;
  }

  virtual void Layout() {
    content_->SetBounds(
        0, kRelatedControlVerticalSpacing,
        width(), height() - kRelatedControlVerticalSpacing);
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) {
    if (is_add && child == this)
      Init();
  }

  // Holds the info icon image and text label and renders the border.
  views::View* content_;
  // Info icon image.
  views::ImageView* info_image_;
  // The label responsible for rendering the text.
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(InfobarView);
};

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
      for_session_blocked_button_(NULL),
      infobar_(NULL) {
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
  HostContentSettingsMap* host_content_settings_map =
      tab_contents_->profile()->GetHostContentSettingsMap();

  // Allowed Cookie list.
  allowed_label_ = new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL)));
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
      UTF16ToWide(l10n_util::GetStringUTF16(
          host_content_settings_map->BlockThirdPartyCookies() ?
              IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED :
              IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL)));
  blocked_label_->SetMultiLine(true);
  blocked_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
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

  GridLayout* layout = GridLayout::CreatePanel(this);
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
      allowed_cookies_tree_, 1, 1, GridLayout::FILL, GridLayout::FILL,
      kTreeViewWidth, kTreeViewHeight);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  block_allowed_button_ = new views::NativeButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_BLOCK_BUTTON)));
  layout->AddView(
      block_allowed_button_, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(
      new views::Separator(), 1, 1, GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(blocked_label_, 1, 1, GridLayout::FILL, GridLayout::FILL);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, single_column_layout_id);
  layout->AddView(
      blocked_cookies_tree_, 1, 1, GridLayout::FILL, GridLayout::FILL,
      kTreeViewWidth, kTreeViewHeight);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, three_columns_layout_id);
  allow_blocked_button_ = new views::NativeButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOW_BUTTON)));
  layout->AddView(allow_blocked_button_);
  for_session_blocked_button_ = new views::NativeButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON)));
  layout->AddView(for_session_blocked_button_);

  layout->StartRow(0, single_column_layout_id);
  infobar_ = new InfobarView();
  layout->AddView(infobar_, 1, 1, GridLayout::FILL, GridLayout::FILL);

  EnableControls();
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedDialogDelegate implementation.

std::wstring CollectedCookiesWin::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE));
}

int CollectedCookiesWin::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring CollectedCookiesWin::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLOSE));
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
// CollectedCookiesWin, private methods.

void CollectedCookiesWin::EnableControls() {
  bool enable_allowed_buttons = false;
  ui::TreeModelNode* node = allowed_cookies_tree_->GetSelectedNode();
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
  infobar_->UpdateVisibility(true, setting, origin_node->GetTitle());
  gfx::Rect bounds;
  GetWidget()->GetBounds(&bounds, false);
  // WidgetWin::GetBounds returns the bounds relative to the parent window,
  // while WidgetWin::SetBounds wants screen coordinates. Do the translation
  // here until http://crbug.com/52851 is fixed.
  POINT topleft = {bounds.x(), bounds.y()};
  MapWindowPoints(HWND_DESKTOP, tab_contents_->GetNativeView(), &topleft, 1);
  gfx::Size size = GetRootView()->GetPreferredSize();
  bounds.SetRect(topleft.x, topleft.y, size.width(), size.height());
  GetWidget()->SetBounds(bounds);
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
