// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/create_application_shortcut_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/codec/png_codec.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif  // defined(OS_WIN)

namespace {

const int kAppIconSize = 32;

bool IconPrecedes(
    const webkit_glue::WebApplicationInfo::IconInfo& left,
    const webkit_glue::WebApplicationInfo::IconInfo& right) {
  return left.width < right.width;
}

// AppInfoView shows the application icon and title.
class AppInfoView : public views::View {
 public:
  AppInfoView(const string16& title,
              const string16& description,
              const SkBitmap& icon);

  // Updates the title/description of the web app.
  void UpdateText(const string16& title, const string16& description);

  // Updates the icon of the web app.
  void UpdateIcon(const SkBitmap& new_icon);

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);

 private:
  // Initializes the controls
  void Init(const string16& title,
            const string16& description, const SkBitmap& icon);

  // Creates or updates description label.
  void PrepareDescriptionLabel(const string16& description);

  // Sets up layout manager.
  void SetupLayout();

  views::ImageView* icon_;
  views::Label* title_;
  views::Label* description_;
};

AppInfoView::AppInfoView(const string16& title,
                         const string16& description,
                         const SkBitmap& icon)
    : icon_(NULL),
      title_(NULL),
      description_(NULL) {
  Init(title, description, icon);
}

void AppInfoView::Init(const string16& title_text,
                       const string16& description_text,
                       const SkBitmap& icon) {
  icon_ = new views::ImageView();
  icon_->SetImage(icon);
  icon_->SetImageSize(gfx::Size(kAppIconSize, kAppIconSize));

  title_ = new views::Label(UTF16ToWide(title_text));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD));

  if (!description_text.empty()) {
    PrepareDescriptionLabel(description_text);
  }

  SetupLayout();
}

void AppInfoView::PrepareDescriptionLabel(const string16& description) {
  DCHECK(!description.empty());

  static const size_t kMaxLength = 200;
  static const wchar_t* const kEllipsis = L" ... ";

  std::wstring text = UTF16ToWide(description);
  if (text.length() > kMaxLength) {
    text = text.substr(0, kMaxLength);
    text += kEllipsis;
  }

  if (description_) {
    description_->SetText(text);
  } else {
    description_ = new views::Label(text);
    description_->SetMultiLine(true);
    description_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  }
}

void AppInfoView::SetupLayout() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  static const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        20.0f, views::GridLayout::FIXED,
                        kAppIconSize, kAppIconSize);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        80.0f, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(icon_, 1, description_ ? 2 : 1);
  layout->AddView(title_);

  if (description_) {
    layout->StartRow(0, kColumnSetId);
    layout->SkipColumns(1);
    layout->AddView(description_);
  }
}

void AppInfoView::UpdateText(const string16& title,
                             const string16& description) {
  title_->SetText(UTF16ToWide(title));
  PrepareDescriptionLabel(description);

  SetupLayout();
}

void AppInfoView::UpdateIcon(const SkBitmap& new_icon) {
  DCHECK(icon_ != NULL);

  icon_->SetImage(new_icon);
}

void AppInfoView::Paint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetLocalBounds(true);

  SkRect border_rect = {
    SkIntToScalar(bounds.x()),
    SkIntToScalar(bounds.y()),
    SkIntToScalar(bounds.right()),
    SkIntToScalar(bounds.bottom())
  };

  SkPaint border_paint;
  border_paint.setAntiAlias(true);
  border_paint.setARGB(0xFF, 0xC8, 0xC8, 0xC8);

  canvas->drawRoundRect(border_rect, SkIntToScalar(2), SkIntToScalar(2),
      border_paint);

  SkRect inner_rect = {
    border_rect.fLeft + SkDoubleToScalar(0.5),
    border_rect.fTop + SkDoubleToScalar(0.5),
    border_rect.fRight - SkDoubleToScalar(0.5),
    border_rect.fBottom - SkDoubleToScalar(0.5),
  };

  SkPaint inner_paint;
  inner_paint.setAntiAlias(true);
  inner_paint.setARGB(0xFF, 0xF8, 0xF8, 0xF8);
  canvas->drawRoundRect(inner_rect, SkIntToScalar(1.5), SkIntToScalar(1.5),
      inner_paint);
}

};  // namespace

namespace browser {

void ShowCreateShortcutsDialog(gfx::NativeWindow parent_window,
                               TabContents* tab_contents) {
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(),
      new CreateApplicationShortcutView(tab_contents))->Show();
}

};  // namespace browser

CreateApplicationShortcutView::CreateApplicationShortcutView(
    TabContents* tab_contents)
    : tab_contents_(tab_contents) {
  Init();
}

CreateApplicationShortcutView::~CreateApplicationShortcutView() {
}

void CreateApplicationShortcutView::Init() {
  // Prepare data
  const webkit_glue::WebApplicationInfo& app_info =
      tab_contents_->web_app_info();

  url_ = app_info.app_url.is_empty() ? tab_contents_->GetURL() :
                                       app_info.app_url;
  title_ = app_info.title.empty() ? tab_contents_->GetTitle() :
                                    app_info.title;
  description_ = app_info.description;

  icon_ = tab_contents_->GetFavIcon();
  if (!app_info.icons.empty()) {
    SetIconsInfo(app_info.icons);
    FetchIcon();
  }

  if (title_.empty())
    title_ = UTF8ToUTF16(url_.spec());

  // Create controls
  app_info_ = new AppInfoView(title_, description_, icon_);
  create_shortcuts_label_ = new views::Label(
      l10n_util::GetString(IDS_CREATE_SHORTCUTS_LABEL));
  create_shortcuts_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  Profile* profile = tab_contents_->profile();

  desktop_check_box_ = AddCheckbox(
      l10n_util::GetString(IDS_CREATE_SHORTCUTS_DESKTOP_CHKBOX),
      profile->GetPrefs()->GetBoolean(prefs::kWebAppCreateOnDesktop));

  menu_check_box_ = NULL;
  quick_launch_check_box_ = NULL;

#if defined(OS_WIN)
  menu_check_box_ = AddCheckbox(
      l10n_util::GetString(IDS_CREATE_SHORTCUTS_START_MENU_CHKBOX),
      profile->GetPrefs()->GetBoolean(prefs::kWebAppCreateInAppsMenu));

  quick_launch_check_box_ = AddCheckbox(
      (win_util::GetWinVersion() >= win_util::WINVERSION_WIN7) ?
        l10n_util::GetString(IDS_PIN_TO_TASKBAR_CHKBOX) :
        l10n_util::GetString(IDS_CREATE_SHORTCUTS_QUICK_LAUNCH_BAR_CHKBOX),
      profile->GetPrefs()->GetBoolean(prefs::kWebAppCreateInQuickLaunchBar));
#elif defined(OS_LINUX)
  menu_check_box_ = AddCheckbox(
      l10n_util::GetString(IDS_CREATE_SHORTCUTS_MENU_CHKBOX),
      profile->GetPrefs()->GetBoolean(prefs::kWebAppCreateInAppsMenu));
#endif

  // Layout controls
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  static const int kHeaderColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kHeaderColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        100.0f, views::GridLayout::FIXED, 0, 0);

  static const int kTableColumnSetId = 1;
  column_set = layout->AddColumnSet(kTableColumnSetId);
  column_set->AddPaddingColumn(5.0f, 10);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        100.0f, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(app_info_);

  layout->AddPaddingRow(0, kPanelSubVerticalSpacing);
  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(create_shortcuts_label_);

  layout->AddPaddingRow(0, kLabelToControlVerticalSpacing);
  layout->StartRow(0, kTableColumnSetId);
  layout->AddView(desktop_check_box_);

  if (menu_check_box_ != NULL) {
    layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
    layout->StartRow(0, kTableColumnSetId);
    layout->AddView(menu_check_box_);
  }

  if (quick_launch_check_box_ != NULL) {
    layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
    layout->StartRow(0, kTableColumnSetId);
    layout->AddView(quick_launch_check_box_);
  }
}

gfx::Size CreateApplicationShortcutView::GetPreferredSize() {
  static const int kDialogWidth = 360;
  int height = GetLayoutManager()->GetPreferredHeightForWidth(this,
      kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

std::wstring CreateApplicationShortcutView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_CREATE_SHORTCUTS_COMMIT);
  }

  return std::wstring();
}

bool CreateApplicationShortcutView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return desktop_check_box_->checked() ||
           ((menu_check_box_ != NULL) &&
            menu_check_box_->checked()) ||
           ((quick_launch_check_box_ != NULL) &&
            quick_launch_check_box_->checked());

  return true;
}

bool CreateApplicationShortcutView::CanResize() const {
  return false;
}

bool CreateApplicationShortcutView::CanMaximize() const {
  return false;
}

bool CreateApplicationShortcutView::IsAlwaysOnTop() const {
  return false;
}

bool CreateApplicationShortcutView::HasAlwaysOnTopMenu() const {
  return false;
}

bool CreateApplicationShortcutView::IsModal() const {
  return true;
}

std::wstring CreateApplicationShortcutView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_CREATE_SHORTCUTS_TITLE);
}

bool CreateApplicationShortcutView::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    return false;
  }

  ShellIntegration::ShortcutInfo shortcut_info;
  shortcut_info.url = url_;
  shortcut_info.title = title_;
  shortcut_info.description = description_;
  shortcut_info.favicon = icon_;
  shortcut_info.create_on_desktop = desktop_check_box_->checked();
  shortcut_info.create_in_applications_menu = menu_check_box_ == NULL ? false :
      menu_check_box_->checked();

#if defined(OS_WIN)
  shortcut_info.create_in_quick_launch_bar = quick_launch_check_box_ == NULL ?
      NULL : quick_launch_check_box_->checked();
#elif defined(OS_POSIX)
  // Create shortcut in Mac dock or as Linux (gnome/kde) application launcher
  // are not implemented yet.
  shortcut_info.create_in_quick_launch_bar = false;
#endif

  web_app::CreateShortcut(
    tab_contents_->profile()->GetPath().Append(chrome::kWebAppDirname),
    shortcut_info, NULL);

  if (tab_contents_->delegate())
    tab_contents_->delegate()->ConvertContentsToApplication(tab_contents_);
  return true;
}

views::View* CreateApplicationShortcutView::GetContentsView() {
  return this;
}

views::Checkbox* CreateApplicationShortcutView::AddCheckbox(
    const std::wstring& text, bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  checkbox->set_listener(this);
  return checkbox;
}

void CreateApplicationShortcutView::SetIconsInfo(const IconInfoList& icons) {
  unprocessed_icons_.clear();
  for (size_t i = 0; i < icons.size(); ++i) {
    // We only take square shaped icons (i.e. width == height).
    if (icons[i].width == icons[i].height) {
      unprocessed_icons_.push_back(icons[i]);
    }
  }

  std::sort(unprocessed_icons_.begin(), unprocessed_icons_.end(),
            &IconPrecedes);
}

void CreateApplicationShortcutView::FetchIcon() {
  // There should only be fetch job at a time.
  DCHECK(icon_fetcher_ == NULL);

  if (unprocessed_icons_.empty()) {
    // No icons to fetch.
    return;
  }

  icon_fetcher_.reset(new URLFetcher(unprocessed_icons_.back().url,
                                     URLFetcher::GET,
                                     this));
  DCHECK(icon_fetcher_.get() != NULL);
  unprocessed_icons_.pop_back();

  icon_fetcher_->set_load_flags(icon_fetcher_->load_flags() |
                                net::LOAD_IS_DOWNLOAD);
  icon_fetcher_->set_request_context(
      tab_contents_->profile()->GetRequestContext());
  icon_fetcher_->Start();
}

void CreateApplicationShortcutView::ButtonPressed(views::Button* sender,
                                                  const views::Event& event) {
  Profile* profile = tab_contents_->profile();
  if (sender == desktop_check_box_)
    profile->GetPrefs()->SetBoolean(prefs::kWebAppCreateOnDesktop,
        desktop_check_box_->checked() ? true : false);
  else if (sender == menu_check_box_)
    profile->GetPrefs()->SetBoolean(prefs::kWebAppCreateInAppsMenu,
        menu_check_box_->checked() ? true : false);
  else if (sender == quick_launch_check_box_)
    profile->GetPrefs()->SetBoolean(prefs::kWebAppCreateInQuickLaunchBar,
        quick_launch_check_box_->checked() ? true : false);

  // When no checkbox is checked we should not have the action button enabled.
  GetDialogClientView()->UpdateDialogButtons();
}

void CreateApplicationShortcutView::OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
  // Delete the fetcher on this function's exit.
  scoped_ptr<URLFetcher> clean_up_fetcher(icon_fetcher_.release());

  bool success = status.is_success() && (response_code == 200) && !data.empty();

  if (success) {
    success = gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(data.c_str()),
        data.size(),
        &icon_);

    if (success)
      static_cast<AppInfoView*>(app_info_)->UpdateIcon(icon_);
  }

  if (!success)
    FetchIcon();
}
