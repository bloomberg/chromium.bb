// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/collected_cookies_views.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/cookie_info_view.h"
#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// TODO(markusheintz): This should be removed once the native Windows tabbed
// pane is not used anymore (http://crbug.com/138059)
#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/views/controls/tabbed_pane/native_tabbed_pane_win.h"
#endif

namespace chrome {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowCollectedCookiesDialog(content::WebContents* web_contents) {
  // Deletes itself on close.
  new CollectedCookiesViews(web_contents);
}

}  // namespace chrome

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
#if defined(USE_AURA) || !defined(OS_WIN)
    SkColor border_color = SK_ColorGRAY;
#else
    SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
#endif
    views::Border* border = views::Border::CreateSolidBorder(
        kInfobarBorderSize, border_color);
    content_->set_border(border);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    info_image_ = new views::ImageView();
    info_image_->SetImage(rb.GetImageSkiaNamed(IDR_INFO));
    label_ = new views::Label();
  }
  virtual ~InfobarView() {}

  // Update the visibility of the infobar. If |is_visible| is true, a rule for
  // |setting| on |domain_name| was created.
  void UpdateVisibility(bool is_visible,
                        ContentSetting setting,
                        const string16& domain_name) {
    if (!is_visible) {
      SetVisible(false);
      return;
    }

    string16 label;
    switch (setting) {
      case CONTENT_SETTING_BLOCK:
        label = l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_BLOCK_RULE_CREATED, domain_name);
        break;

      case CONTENT_SETTING_ALLOW:
        label = l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_ALLOW_RULE_CREATED, domain_name);
        break;

      case CONTENT_SETTING_SESSION_ONLY:
        label = l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_SESSION_RULE_CREATED, domain_name);
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
                             views::kRelatedControlSmallHorizontalSpacing));
    content_->AddChildView(info_image_);
    content_->AddChildView(label_);
    UpdateVisibility(false, CONTENT_SETTING_BLOCK, string16());
  }

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() {
    if (!visible())
      return gfx::Size();

    // Add space around the banner.
    gfx::Size size(content_->GetPreferredSize());
    size.Enlarge(0, 2 * views::kRelatedControlVerticalSpacing);
    return size;
  }

  virtual void Layout() {
    content_->SetBounds(
        0, views::kRelatedControlVerticalSpacing,
        width(), height() - views::kRelatedControlVerticalSpacing);
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
// CollectedCookiesViews, public:

CollectedCookiesViews::CollectedCookiesViews(content::WebContents* web_contents)
    : web_contents_(web_contents),
      allowed_label_(NULL),
      blocked_label_(NULL),
      allowed_cookies_tree_(NULL),
      blocked_cookies_tree_(NULL),
      block_allowed_button_(NULL),
      allow_blocked_button_(NULL),
      for_session_blocked_button_(NULL),
      cookie_info_view_(NULL),
      infobar_(NULL),
      status_changed_(false) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  registrar_.Add(this, chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
                 content::Source<TabSpecificContentSettings>(content_settings));
  window_ = new ConstrainedWindowViews(web_contents, this);
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::DialogDelegate implementation:

string16 CollectedCookiesViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE);
}

int CollectedCookiesViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

string16 CollectedCookiesViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

void CollectedCookiesViews::DeleteDelegate() {
  delete this;
}

bool CollectedCookiesViews::Cancel() {
  if (status_changed_) {
    CollectedCookiesInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents_));
  }

  return true;
}

ui::ModalType CollectedCookiesViews::GetModalType() const {
#if defined(USE_ASH)
  return ui::MODAL_TYPE_CHILD;
#else
  return views::WidgetDelegate::GetModalType();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::ButtonListener implementation:

void CollectedCookiesViews::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == block_allowed_button_)
    AddContentException(allowed_cookies_tree_, CONTENT_SETTING_BLOCK);
  else if (sender == allow_blocked_button_)
    AddContentException(blocked_cookies_tree_, CONTENT_SETTING_ALLOW);
  else if (sender == for_session_blocked_button_)
    AddContentException(blocked_cookies_tree_, CONTENT_SETTING_SESSION_ONLY);
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::TabbedPaneListener implementation:

void CollectedCookiesViews::TabSelectedAt(int index) {
  EnableControls();
  ShowCookieInfo();
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::TreeViewController implementation:

void CollectedCookiesViews::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
  EnableControls();
  ShowCookieInfo();
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::View overrides:

void CollectedCookiesViews::ViewHierarchyChanged(bool is_add,
                                                 views::View* parent,
                                                 views::View* child) {
  if (is_add && child == this)
    Init();
}

////////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, private:

CollectedCookiesViews::~CollectedCookiesViews() {
  allowed_cookies_tree_->SetModel(NULL);
  blocked_cookies_tree_->SetModel(NULL);
}

void CollectedCookiesViews::Init() {
  using views::GridLayout;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int single_column_with_padding_layout_id = 1;
  views::ColumnSet* column_set_with_padding = layout->AddColumnSet(
      single_column_with_padding_layout_id);
  column_set_with_padding->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                                     GridLayout::USE_PREF, 0, 0);
  column_set_with_padding->AddPaddingColumn(0, 2);

  layout->StartRow(0, single_column_layout_id);
  views::TabbedPane* tabbed_pane = new views::TabbedPane();
#if defined(OS_WIN) && !defined(USE_AURA)
  // "set_use_native_win_control" must be called before the first tab is added.
  tabbed_pane->set_use_native_win_control(true);
#endif
  layout->AddView(tabbed_pane);
  // NOTE: the panes need to be added after the tabbed_pane has been added to
  // its parent.
  string16 label_allowed = l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_TAB_LABEL);
  string16 label_blocked = l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_TAB_LABEL);
  tabbed_pane->AddTab(label_allowed, CreateAllowedPane());
  tabbed_pane->AddTab(label_blocked, CreateBlockedPane());
  tabbed_pane->SelectTabAt(0);
  tabbed_pane->set_listener(this);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_with_padding_layout_id);
  cookie_info_view_ = new CookieInfoView(false);
  layout->AddView(cookie_info_view_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_with_padding_layout_id);
  infobar_ = new InfobarView();
  layout->AddView(infobar_);

  EnableControls();
  ShowCookieInfo();
}

views::View* CollectedCookiesViews::CreateAllowedPane() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);

  // Create the controls that go into the pane.
  allowed_label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL));

  const LocalSharedObjectsContainer& allowed_data =
      content_settings->allowed_local_shared_objects();
  allowed_cookies_tree_model_ = allowed_data.CreateCookiesTreeModel();
  allowed_cookies_tree_ = new views::TreeView();
  allowed_cookies_tree_->SetModel(allowed_cookies_tree_model_.get());
  allowed_cookies_tree_->SetRootShown(false);
  allowed_cookies_tree_->SetEditable(false);
  allowed_cookies_tree_->set_lines_at_root(true);
  allowed_cookies_tree_->set_auto_expand_children(true);
  allowed_cookies_tree_->SetController(this);

  block_allowed_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_BLOCK_BUTTON));

  // Create the view that holds all the controls together.  This will be the
  // pane added to the tabbed pane.
  using views::GridLayout;

  views::View* pane = new views::View();
  GridLayout* layout = GridLayout::CreatePanel(pane);
  pane->SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(allowed_label_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_layout_id);
  layout->AddView(allowed_cookies_tree_->CreateParentIfNecessary(), 1, 1,
                  GridLayout::FILL, GridLayout::FILL, kTreeViewWidth,
                  kTreeViewHeight);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(block_allowed_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::CENTER);

  return pane;
}

views::View* CollectedCookiesViews::CreateBlockedPane() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  // Create the controls that go into the pane.
  blocked_label_ = new views::Label(
      l10n_util::GetStringUTF16(
          prefs->GetBoolean(prefs::kBlockThirdPartyCookies) ?
              IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED :
              IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL));
  blocked_label_->SetMultiLine(true);
  blocked_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const LocalSharedObjectsContainer& blocked_data =
      content_settings->blocked_local_shared_objects();
  blocked_cookies_tree_model_ = blocked_data.CreateCookiesTreeModel();
  blocked_cookies_tree_ = new views::TreeView();
  blocked_cookies_tree_->SetModel(blocked_cookies_tree_model_.get());
  blocked_cookies_tree_->SetRootShown(false);
  blocked_cookies_tree_->SetEditable(false);
  blocked_cookies_tree_->set_lines_at_root(true);
  blocked_cookies_tree_->set_auto_expand_children(true);
  blocked_cookies_tree_->SetController(this);

  allow_blocked_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOW_BUTTON));
  for_session_blocked_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON));

  // Create the view that holds all the controls together.  This will be the
  // pane added to the tabbed pane.
  using views::GridLayout;

  views::View* pane = new views::View();
  GridLayout* layout = GridLayout::CreatePanel(pane);
  pane->SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int three_columns_layout_id = 1;
  column_set = layout->AddColumnSet(three_columns_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(blocked_label_, 1, 1, GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_layout_id);
  layout->AddView(
      blocked_cookies_tree_->CreateParentIfNecessary(), 1, 1,
      GridLayout::FILL, GridLayout::FILL, kTreeViewWidth, kTreeViewHeight);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, three_columns_layout_id);
  layout->AddView(allow_blocked_button_);
  layout->AddView(for_session_blocked_button_);

  return pane;
}

void CollectedCookiesViews::EnableControls() {
  bool enable_allowed_buttons = false;
  ui::TreeModelNode* node = allowed_cookies_tree_->GetSelectedNode();
  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_HOST) {
      enable_allowed_buttons = static_cast<CookieTreeHostNode*>(
          cookie_node)->CanCreateContentException();
    }
  }
  block_allowed_button_->SetEnabled(enable_allowed_buttons);

  bool enable_blocked_buttons = false;
  node = blocked_cookies_tree_->GetSelectedNode();
  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_HOST) {
      enable_blocked_buttons = static_cast<CookieTreeHostNode*>(
          cookie_node)->CanCreateContentException();
    }
  }
  allow_blocked_button_->SetEnabled(enable_blocked_buttons);
  for_session_blocked_button_->SetEnabled(enable_blocked_buttons);
}

void CollectedCookiesViews::ShowCookieInfo() {
  ui::TreeModelNode* node = allowed_cookies_tree_->GetSelectedNode();
  if (!node)
    node = blocked_cookies_tree_->GetSelectedNode();

  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    const CookieTreeNode::DetailedInfo detailed_info =
        cookie_node->GetDetailedInfo();

    if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
      cookie_info_view_->SetCookie(detailed_info.cookie->Domain(),
                                   *detailed_info.cookie);
    } else {
      cookie_info_view_->ClearCookieDisplay();
    }
  } else {
    cookie_info_view_->ClearCookieDisplay();
  }
}

void CollectedCookiesViews::AddContentException(views::TreeView* tree_view,
                                                ContentSetting setting) {
  CookieTreeHostNode* host_node =
      static_cast<CookieTreeHostNode*>(tree_view->GetSelectedNode());
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  host_node->CreateContentException(
      CookieSettings::Factory::GetForProfile(profile), setting);
  infobar_->UpdateVisibility(true, setting, host_node->GetTitle());
  status_changed_ = true;
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, content::NotificationObserver implementation:

void CollectedCookiesViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN, type);
  window_->CloseWebContentsModalDialog();
}
