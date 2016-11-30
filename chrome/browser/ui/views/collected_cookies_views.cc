// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/collected_cookies_views.h"

#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/views/cookie_info_view.h"
#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/browser/ui/views/layout_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Spacing between the infobar frame and its contents.
const int kInfobarVerticalPadding = 3;
const int kInfobarHorizontalPadding = 8;

// Width of the infobar frame.
const int kInfobarBorderSize = 1;

// Dimensions of the tree views.
const int kTreeViewWidth = 400;
const int kTreeViewHeight = 125;

// The color of the border around the cookies tree view.
const SkColor kCookiesBorderColor = SkColorSetRGB(0xC8, 0xC8, 0xC8);

// Spacing constants used with non-Harmony dialogs.
const int kTabbedPaneTopPadding = 14;
const int kCookieInfoBottomPadding = 4;

LayoutDelegate::LayoutDistanceType GetTreeviewToButtonsDistanceType() {
  // Hack: in the Harmony specs, the buttons under the treeview are "unrelated"
  // to it (which looks better), but in the existing dialog they were related.
  return LayoutDelegate::Get()->IsHarmonyMode()
             ? LayoutDelegate::LayoutDistanceType::
                   UNRELATED_CONTROL_VERTICAL_SPACING
             : LayoutDelegate::LayoutDistanceType::
                   RELATED_CONTROL_VERTICAL_SPACING;
}

}  // namespace

// A custom view that conditionally displays an infobar.
class InfobarView : public views::View {
 public:
  InfobarView() {
    content_ = new views::View;
    SkColor border_color = SK_ColorGRAY;
    content_->SetBorder(
        views::CreateSolidBorder(kInfobarBorderSize, border_color));

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    info_image_ = new views::ImageView();
    info_image_->SetImage(rb.GetImageSkiaNamed(IDR_INFO));
    label_ = new views::Label();
  }
  ~InfobarView() override {}

  // Update the visibility of the infobar. If |is_visible| is true, a rule for
  // |setting| on |domain_name| was created.
  void UpdateVisibility(bool is_visible,
                        ContentSetting setting,
                        const base::string16& domain_name) {
    if (!is_visible) {
      SetVisible(false);
      return;
    }

    base::string16 label;
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
    UpdateVisibility(false, CONTENT_SETTING_BLOCK, base::string16());
  }

  // views::View overrides.
  gfx::Size GetPreferredSize() const override {
    // Always return the preferred size, even if not currently visible. This
    // ensures that the layout manager always reserves space within the view
    // so it can be made visible when necessary. Otherwise, changing the
    // visibility of this view would require the entire dialog to be resized,
    // which is undesirable from both a UX and technical perspective.

    // Add space around the banner.
    gfx::Size size(content_->GetPreferredSize());
    size.Enlarge(0, 2 * views::kRelatedControlVerticalSpacing);
    return size;
  }

  void Layout() override {
    content_->SetBounds(
        0, views::kRelatedControlVerticalSpacing,
        width(), height() - views::kRelatedControlVerticalSpacing);
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this)
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
      delete_allowed_button_(NULL),
      allow_blocked_button_(NULL),
      for_session_blocked_button_(NULL),
      cookie_info_view_(NULL),
      infobar_(NULL),
      status_changed_(false) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  registrar_.Add(this, chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
                 content::Source<TabSpecificContentSettings>(content_settings));
  constrained_window::ShowWebModalDialogViews(this, web_contents);
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::DialogDelegate implementation:

base::string16 CollectedCookiesViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE);
}

int CollectedCookiesViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CollectedCookiesViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool CollectedCookiesViews::Cancel() {
  // If the user closes our parent tab while we're still open, this method will
  // (eventually) be called in response to a WebContentsDestroyed() call from
  // the WebContentsImpl to its observers.  But since the InfoBarService is also
  // torn down in response to WebContentsDestroyed(), it may already be null.
  // Since the tab is going away anyway, we can just omit showing an infobar,
  // which prevents any attempt to access a null InfoBarService.
  if (status_changed_ && !web_contents_->IsBeingDestroyed()) {
    CollectedCookiesInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents_));
  }
  return true;
}

ui::ModalType CollectedCookiesViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::ButtonListener implementation:

void CollectedCookiesViews::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == block_allowed_button_) {
    AddContentException(allowed_cookies_tree_, CONTENT_SETTING_BLOCK);
  } else if (sender == delete_allowed_button_) {
    allowed_cookies_tree_model_->DeleteCookieNode(
        static_cast<CookieTreeNode*>(allowed_cookies_tree_->GetSelectedNode()));
  } else if (sender == allow_blocked_button_) {
    AddContentException(blocked_cookies_tree_, CONTENT_SETTING_ALLOW);
  } else if (sender == for_session_blocked_button_) {
    AddContentException(blocked_cookies_tree_, CONTENT_SETTING_SESSION_ONLY);
  }
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

gfx::Size CollectedCookiesViews::GetMinimumSize() const {
  // Allow UpdateWebContentsModalDialogPosition to clamp the dialog width.
  return gfx::Size(0, View::GetMinimumSize().height());
}

gfx::Size CollectedCookiesViews::GetPreferredSize() const {
  int preferred = LayoutDelegate::Get()->GetDialogPreferredWidth(
      LayoutDelegate::DialogWidthType::MEDIUM);
  return gfx::Size(preferred ? preferred : View::GetPreferredSize().width(),
                   View::GetPreferredSize().height());
}

void CollectedCookiesViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this)
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

  GridLayout* layout = new GridLayout(this);
  if (LayoutDelegate::Get()->UseExtraDialogPadding())
    layout->SetInsets(gfx::Insets(kTabbedPaneTopPadding, 0, 0, 0));
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  views::TabbedPane* tabbed_pane = new views::TabbedPane();

  layout->AddView(tabbed_pane);
  // NOTE: Panes must be added after |tabbed_pane| has been added to its parent.
  base::string16 label_allowed = l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_TAB_LABEL);
  base::string16 label_blocked = l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_TAB_LABEL);
  tabbed_pane->AddTab(label_allowed, CreateAllowedPane());
  tabbed_pane->AddTab(label_blocked, CreateBlockedPane());
  tabbed_pane->SelectTabAt(0);
  tabbed_pane->set_listener(this);
  if (LayoutDelegate::Get()->UseExtraDialogPadding()) {
    layout->AddPaddingRow(0, LayoutDelegate::Get()->GetLayoutDistance(
                                 LayoutDelegate::LayoutDistanceType::
                                     RELATED_CONTROL_VERTICAL_SPACING));
  }

  layout->StartRow(0, single_column_layout_id);
  cookie_info_view_ = new CookieInfoView();
  layout->AddView(cookie_info_view_);
  if (LayoutDelegate::Get()->UseExtraDialogPadding())
    layout->AddPaddingRow(0, kCookieInfoBottomPadding);

  layout->StartRow(0, single_column_layout_id);
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
  allowed_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  allowed_cookies_tree_model_ =
      content_settings->allowed_local_shared_objects().CreateCookiesTreeModel();
  allowed_cookies_tree_ = new views::TreeView();
  allowed_cookies_tree_->SetModel(allowed_cookies_tree_model_.get());
  allowed_cookies_tree_->SetRootShown(false);
  allowed_cookies_tree_->SetEditable(false);
  allowed_cookies_tree_->set_auto_expand_children(true);
  allowed_cookies_tree_->SetController(this);

  block_allowed_button_ = views::MdTextButton::CreateSecondaryUiButton(this,
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_BLOCK_BUTTON));

  delete_allowed_button_ = views::MdTextButton::CreateSecondaryUiButton(this,
      l10n_util::GetStringUTF16(IDS_COOKIES_REMOVE_LABEL));

  // Create the view that holds all the controls together.  This will be the
  // pane added to the tabbed pane.
  using views::GridLayout;

  views::View* pane = new views::View();
  GridLayout* layout = layout_utils::CreatePanelLayout(pane);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int three_columns_layout_id = 1;
  column_set = layout->AddColumnSet(three_columns_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, LayoutDelegate::Get()->GetLayoutDistance(
                                      LayoutDelegate::LayoutDistanceType::
                                          RELATED_BUTTON_HORIZONTAL_SPACING));
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(allowed_label_);
  layout->AddPaddingRow(0, LayoutDelegate::Get()->GetLayoutDistance(
                               LayoutDelegate::LayoutDistanceType::
                                   UNRELATED_CONTROL_VERTICAL_SPACING));

  layout->StartRow(1, single_column_layout_id);
  layout->AddView(CreateScrollView(allowed_cookies_tree_), 1, 1,
                  GridLayout::FILL, GridLayout::FILL, kTreeViewWidth,
                  kTreeViewHeight);
  layout->AddPaddingRow(0, LayoutDelegate::Get()->GetLayoutDistance(
                               GetTreeviewToButtonsDistanceType()));

  layout->StartRow(0, three_columns_layout_id);
  layout->AddView(block_allowed_button_);
  layout->AddView(delete_allowed_button_);

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
  blocked_label_->SizeToFit(kTreeViewWidth);
  blocked_cookies_tree_model_ =
      content_settings->blocked_local_shared_objects().CreateCookiesTreeModel();
  blocked_cookies_tree_ = new views::TreeView();
  blocked_cookies_tree_->SetModel(blocked_cookies_tree_model_.get());
  blocked_cookies_tree_->SetRootShown(false);
  blocked_cookies_tree_->SetEditable(false);
  blocked_cookies_tree_->set_auto_expand_children(true);
  blocked_cookies_tree_->SetController(this);

  allow_blocked_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOW_BUTTON));
  for_session_blocked_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON));

  // Create the view that holds all the controls together.  This will be the
  // pane added to the tabbed pane.
  using views::GridLayout;

  views::View* pane = new views::View();
  GridLayout* layout = layout_utils::CreatePanelLayout(pane);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int three_columns_layout_id = 1;
  column_set = layout->AddColumnSet(three_columns_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, LayoutDelegate::Get()->GetLayoutDistance(
                                      LayoutDelegate::LayoutDistanceType::
                                          RELATED_BUTTON_HORIZONTAL_SPACING));
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(blocked_label_, 1, 1, GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, LayoutDelegate::Get()->GetLayoutDistance(
                               LayoutDelegate::LayoutDistanceType::
                                   UNRELATED_CONTROL_VERTICAL_SPACING));

  layout->StartRow(1, single_column_layout_id);
  layout->AddView(
      CreateScrollView(blocked_cookies_tree_), 1, 1,
      GridLayout::FILL, GridLayout::FILL, kTreeViewWidth, kTreeViewHeight);
  layout->AddPaddingRow(0, LayoutDelegate::Get()->GetLayoutDistance(
                               GetTreeviewToButtonsDistanceType()));

  layout->StartRow(0, three_columns_layout_id);
  layout->AddView(allow_blocked_button_);
  layout->AddView(for_session_blocked_button_);

  return pane;
}

views::View* CollectedCookiesViews::CreateScrollView(views::TreeView* pane) {
  views::ScrollView* scroll_view = new views::ScrollView();
  scroll_view->SetContents(pane);
  scroll_view->SetBorder(views::CreateSolidBorder(1, kCookiesBorderColor));
  return scroll_view;
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
  delete_allowed_button_->SetEnabled(node != NULL);

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
  ui::TreeModelNode* node = allowed_cookies_tree_->IsDrawn() ?
                            allowed_cookies_tree_->GetSelectedNode() : nullptr;

  if (!node && blocked_cookies_tree_->IsDrawn())
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
      CookieSettingsFactory::GetForProfile(profile).get(), setting);
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
  GetWidget()->Close();
}
