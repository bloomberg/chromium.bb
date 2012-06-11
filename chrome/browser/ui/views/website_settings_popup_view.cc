// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings_popup_view.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/content_settings_types.h"
#include "content/public/browser/cert_store.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace {

// In order to make the arrow of the bubble point directly at the location icon
// in the Omnibox rather then the bottom border of the Omnibox, the position of
// the bubble must be adjusted. This is the number of pixel the bubble must be
// moved towards the top of the screen (starting from the bottom border of the
// Omnibox).
const int kLocationIconBottomMargin = 5;

const int kPopupWidth = 300;

string16 PermissionTypeToString(ContentSettingsType type) {
  return l10n_util::GetStringUTF16(
      WebsiteSettingsUI::PermissionTypeToUIStringID(type));
}

string16 PermissionValueToString(ContentSetting value) {
  return l10n_util::GetStringUTF16(
      WebsiteSettingsUI::PermissionValueToUIStringID(value));
}

}  // namespace

namespace website_settings {

// A |ComboboxModel| implementation that is used for |Combobox|es that allow
// selecting a setting for a given site permission.
class PermissionComboboxModel : public ui::ComboboxModel {
 public:
  // Creates a combobox model that provides all possible settings for the given
  // |site_permission|.
  PermissionComboboxModel(ContentSettingsType site_permission,
                          ContentSetting default_setting);
  virtual ~PermissionComboboxModel();

  // Returns the setting for the given |index|.
  ContentSetting GetSettingAt(int index) const;

  // Returns the site permission for which the combobox model provides
  // settings.
  ContentSettingsType site_permission() const {
    return site_permission_;
  }

  // ui::ComboboxModel implementations.
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  // The site permission (the |ContentSettingsType|) for which the combobox
  // model provides settings.
  ContentSettingsType site_permission_;

  // The global default setting for the |site_permission_|.
  ContentSetting default_setting_;

  // All possible valid setting for the |site_permission_|.
  std::vector<ContentSetting> settings_;

  DISALLOW_COPY_AND_ASSIGN(PermissionComboboxModel);
};

PermissionComboboxModel::PermissionComboboxModel(
    ContentSettingsType site_permission,
    ContentSetting default_setting)
    : site_permission_(site_permission),
      default_setting_(default_setting){
  settings_.push_back(CONTENT_SETTING_DEFAULT);
  settings_.push_back(CONTENT_SETTING_ALLOW);
  settings_.push_back(CONTENT_SETTING_BLOCK);
  if (site_permission == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
      site_permission == CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    settings_.push_back(CONTENT_SETTING_ASK);
}

PermissionComboboxModel::~PermissionComboboxModel() {
}

ContentSetting PermissionComboboxModel::GetSettingAt(int index) const {
  if (index < static_cast<int>(settings_.size()))
    return settings_[index];
  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

int PermissionComboboxModel::GetItemCount() const {
 return settings_.size();
}

string16 PermissionComboboxModel::GetItemAt(int index) {
  if (index == 0) {
    return l10n_util::GetStringFUTF16(
        IDS_WEBSITE_SETTINGS_DEFAULT_PERMISSION_LABEL,
        PermissionValueToString(default_setting_));
  }
  if (index < static_cast<int>(settings_.size())) {
    return l10n_util::GetStringFUTF16(
        IDS_WEBSITE_SETTINGS_PERMISSION_LABEL,
        PermissionValueToString(settings_[index]));
  }
  NOTREACHED();
  return string16();
}

}  // namespace website_settings

// |PopupHeader| is the UI element (view) that represents the header of the
// |WebsiteSettingsPopupView|. The header shows the status of the site's
// identity check and the name of the site's identity.
class PopupHeader : public views::View {
 public:
  PopupHeader();
  virtual ~PopupHeader();

  // Sets the name of the site's identity.
  void SetIdentityName(const string16& name);

  // Sets the status text for the identity check of this site.
  void SetIdentityStatus(const string16& status_text);

 private:
  // The label that displays the name of the site's identity.
  views::Label* name_;
  // The label that displays the status of the identity check for this site.
  views::Label* status_;

  DISALLOW_COPY_AND_ASSIGN(PopupHeader);
};

////////////////////////////////////////////////////////////////////////////////
// Popup Header
////////////////////////////////////////////////////////////////////////////////

PopupHeader::PopupHeader() : name_(NULL), status_(NULL) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int label_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(label_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->AddPaddingRow(0, 8);

  layout->StartRow(0, label_column);
  name_ = new views::Label(string16());
  name_->SetFont(name_->font().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(name_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  layout->AddPaddingRow(0, 4);

  status_ = new views::Label(string16());
  layout->StartRow(0, label_column);
  layout->AddView(status_,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
}

PopupHeader::~PopupHeader() {
}

void PopupHeader::SetIdentityName(const string16& name) {
  name_->SetText(name);
}

void PopupHeader::SetIdentityStatus(const string16& status) {
  status_->SetText(status);
}

///////////////////////////////////////////////////////////////////////////////
// WebsiteSettingsPopupView
///////////////////////////////////////////////////////////////////////////////

WebsiteSettingsPopupView::~WebsiteSettingsPopupView() {
}

// static
void WebsiteSettingsPopupView::ShowPopup(views::View* anchor_view,
                                         Profile* profile,
                                         TabContents* tab_contents,
                                         const GURL& url,
                                         const content::SSLStatus& ssl) {
  new WebsiteSettingsPopupView(anchor_view, profile, tab_contents, url, ssl);
}

WebsiteSettingsPopupView::WebsiteSettingsPopupView(
    views::View* anchor_view,
    Profile* profile,
    TabContents* tab_contents,
    const GURL& url,
    const content::SSLStatus& ssl)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      tab_contents_(tab_contents),
      header_(NULL),
      tabbed_pane_(NULL),
      site_data_content_(NULL),
      cookie_dialog_link_(NULL),
      permissions_content_(NULL),
      identity_info_text_(NULL),
      connection_info_text_(NULL),
      page_info_text_(NULL) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  header_ = new PopupHeader();
  layout->StartRow(1, content_column);
  layout->AddView(header_);

  layout->AddPaddingRow(1, 10);

  tabbed_pane_ = new views::TabbedPane();
  layout->StartRow(1, content_column);
  layout->AddView(tabbed_pane_);
  // Tabs must be added after the tabbed_pane_ was added to the views hierachy.
  // Adding the |tabbed_pane_| to the views hierachy triggers the
  // initialization of the native tab UI element. If the native tab UI element
  // is not initalized adding a tab will result in a NULL pointer excetion.
  tabbed_pane_->AddTab(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS),
      CreatePermissionsTab());
  tabbed_pane_->AddTab(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TAB_LABEL_IDENTITY),
      CreateIdentityTab());
  tabbed_pane_->SelectTabAt(0);
  tabbed_pane_->set_listener(this);

  views::BubbleDelegateView::CreateBubble(this);
  this->Show();
  SizeToContents();

  presenter_.reset(new WebsiteSettings(this, profile,
                                       tab_contents->content_settings(), url,
                                       ssl, content::CertStore::GetInstance()));
}

void WebsiteSettingsPopupView::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  site_data_content_->RemoveAllChildViews(true);

  views::GridLayout* layout = new views::GridLayout(site_data_content_);
  site_data_content_->SetLayoutManager(layout);

  const int site_data_content_column = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(site_data_content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  for (CookieInfoList::const_iterator it = cookie_info_list.begin();
       it != cookie_info_list.end();
       ++it) {
    string16 label_text = l10n_util::GetStringFUTF16(
        IDS_WEBSITE_SETTINGS_SITE_DATA_STATS_LINE,
        UTF8ToUTF16(it->cookie_source),
        base::IntToString16(it->allowed),
        base::IntToString16(it->allowed));
    layout->StartRow(1, site_data_content_column);
    layout->AddView(new views::Label(label_text),
                    1,
                    1,
                    views::GridLayout::LEADING,
                    views::GridLayout::CENTER);

    layout->AddPaddingRow(1, 4);
  }

  layout->Layout(site_data_content_);
  SizeToContents();
}

void WebsiteSettingsPopupView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  permissions_content_->RemoveAllChildViews(true);

  views::GridLayout* layout =
      new views::GridLayout(permissions_content_);
  permissions_content_->SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  for (PermissionInfoList::const_iterator permission =
           permission_info_list.begin();
       permission != permission_info_list.end();
       ++permission) {
    views::Label* label =
        new views::Label(PermissionTypeToString(permission->type));

    // The |ComboboxModel| is not owned by the |Combobox|. Therefore all models
    // are stored in a scoped vector so that they are be deleted when the popup
    // is destroyed.
    combobox_models_->push_back(new website_settings::PermissionComboboxModel(
          permission->type, permission->default_setting));
    views::Combobox* combobox = new views::Combobox(
        combobox_models_->back());
    switch (permission->setting) {
      case CONTENT_SETTING_DEFAULT:
        combobox->SetSelectedIndex(0);
        break;
      case CONTENT_SETTING_ALLOW:
        combobox->SetSelectedIndex(1);
        break;
      case CONTENT_SETTING_BLOCK:
        combobox->SetSelectedIndex(2);
        break;
      default:
        combobox->SetSelectedIndex(4);
        break;
    }
    combobox->set_listener(this);

    layout->StartRow(1, content_column);
    layout->AddView(CreatePermissionRow(label, combobox),
                    1,
                    1,
                    views::GridLayout::LEADING,
                    views::GridLayout::CENTER);

  }

  SizeToContents();
}

void WebsiteSettingsPopupView::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  string16 identity_status_text;
  switch (identity_info.identity_status) {
    case WebsiteSettings::SITE_IDENTITY_STATUS_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_DNSSEC_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT:
      identity_status_text =
          l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_IDENTITY_VERIFIED);
      break;
    default:
      identity_status_text =
         l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_IDENTITY_NOT_VERIFIED);
      break;
  }
  header_->SetIdentityName(UTF8ToUTF16(identity_info.site_identity));
  header_->SetIdentityStatus(identity_status_text);

  identity_info_text_->SetText(
      UTF8ToUTF16(identity_info.identity_status_description));

  connection_info_text_->SetText(
      UTF8ToUTF16(identity_info.connection_status_description));

  GetLayoutManager()->Layout(this);
  SizeToContents();
}

gfx::Size WebsiteSettingsPopupView::GetPreferredSize() {
  int height = 0;
  if (header_)
    height += header_->GetPreferredSize().height();
  if (tabbed_pane_)
    height += tabbed_pane_->GetPreferredSize().height();

  return gfx::Size(kPopupWidth, height);
}

void WebsiteSettingsPopupView::SetFirstVisit(const string16& first_visit) {
  page_info_text_->SetText(first_visit);
}

gfx::Rect WebsiteSettingsPopupView::GetAnchorRect() {
  // Compensate for some built-in padding in the icon. This will make the arrow
  // point to the middle of the icon.
  gfx::Rect anchor(BubbleDelegateView::GetAnchorRect());
  anchor.Inset(0, anchor_view() ? kLocationIconBottomMargin : 0);
  return anchor;
}

void WebsiteSettingsPopupView::OnSelectedIndexChanged(
    views::Combobox* combobox) {
  website_settings::PermissionComboboxModel* model =
    static_cast<website_settings::PermissionComboboxModel*>(combobox->model());
  DCHECK(model);
  presenter_->OnSitePermissionChanged(
      model->site_permission(),
      model->GetSettingAt(combobox->selected_index()));
}

void WebsiteSettingsPopupView::LinkClicked(views::Link* source,
                                           int event_flags) {
  DCHECK_EQ(cookie_dialog_link_, source);
  new CollectedCookiesViews(tab_contents_);
  GetWidget()->CloseNow();
}

void WebsiteSettingsPopupView::TabSelectedAt(int index) {
  tabbed_pane_->GetSelectedTab()->Layout();
  SizeToContents();
}

views::View* WebsiteSettingsPopupView::CreatePermissionsTab() {
  views::View* pane = new views::View();
  pane->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

  cookie_dialog_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_SHOW_SITE_DATA));
  cookie_dialog_link_->set_listener(this);
  site_data_content_ = new views::View();
  views::View* site_data_section =
      CreateSection(l10n_util::GetStringUTF16(
                        IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA),
                    site_data_content_,
                    cookie_dialog_link_);
  pane->AddChildView(site_data_section);

  permissions_content_ = new views::View();
  views::View* permissions_section =
      CreateSection(l10n_util::GetStringUTF16(
                        IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS),
                    permissions_content_,
                    NULL);
  pane->AddChildView(permissions_section);
  return pane;
}

views::View* WebsiteSettingsPopupView::CreateIdentityTab() {
  views::View* pane = new views::View();
  pane->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

  // Add Identity section.
  views::View* section_content = new views::View();
  views::GridLayout* layout =
      new views::GridLayout(section_content);
  section_content->SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  identity_info_text_ = CreateTextLabel(string16());
  layout->StartRow(1, content_column);
  layout->AddView(identity_info_text_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  views::View* section =
      CreateSection(l10n_util::GetStringUTF16(
                        IDS_WEBSITE_SETTINGS_TITEL_IDENTITY),
                    section_content,
                    NULL);
  pane->AddChildView(section);

  // Add connection section.
  section_content = new views::View();
  layout = new views::GridLayout(section_content);
  section_content->SetLayoutManager(layout);
  column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  connection_info_text_ = CreateTextLabel(string16());
  layout->StartRow(1, content_column);
  layout->AddView(connection_info_text_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  section = CreateSection(l10n_util::GetStringUTF16(
                              IDS_WEBSITE_SETTINGS_TITEL_CONNECTION),
                          section_content,
                          NULL);
  pane->AddChildView(section);

  // Add page info section.
  section_content = new views::View();
  layout = new views::GridLayout(section_content);
  section_content->SetLayoutManager(layout);
  column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  page_info_text_ = CreateTextLabel(string16());
  layout->StartRow(1, content_column);
  layout->AddView(page_info_text_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  section = CreateSection(l10n_util::GetStringUTF16(
                              IDS_PAGE_INFO_SITE_INFO_TITLE),
                          section_content,
                          NULL);
  pane->AddChildView(section);

  return pane;
}

views::View* WebsiteSettingsPopupView::CreateSection(
    const string16& headline_text,
    views::View* content,
    views::Link* link) {
  views::View* container = new views::View();
  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->AddPaddingRow(1, 4);
  layout->StartRow(1, content_column);
  views::Label* headline = new views::Label(headline_text);
  headline->SetFont(headline->font().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(headline, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  layout->AddPaddingRow(1, 4);
  layout->StartRow(1, content_column);
  layout->AddView(content, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  if (link) {
    layout->AddPaddingRow(1, 4);
    layout->StartRow(1, content_column);
    layout->AddView(link, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }

  return container;
}

views::View* WebsiteSettingsPopupView::CreatePermissionRow(
    views::Label* label,
    views::Combobox* combobox) {
  views::View* container = new views::View();
  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);
  const int two_column_layout = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(two_column_layout);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->StartRow(1, two_column_layout);
  layout->AddView(label);
  layout->AddView(combobox);
  return container;
}

views::Label* WebsiteSettingsPopupView::CreateTextLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}
