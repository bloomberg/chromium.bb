// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/create_application_shortcut_view.h"

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/codec/png_codec.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

namespace {

const int kAppIconSize = 32;

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
  virtual void OnPaint(gfx::Canvas* canvas);

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
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
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

void AppInfoView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetLocalBounds();

  SkRect border_rect = {
    SkIntToScalar(bounds.x()),
    SkIntToScalar(bounds.y()),
    SkIntToScalar(bounds.right()),
    SkIntToScalar(bounds.bottom())
  };

  SkPaint border_paint;
  border_paint.setAntiAlias(true);
  border_paint.setARGB(0xFF, 0xC8, 0xC8, 0xC8);

  canvas->AsCanvasSkia()->drawRoundRect(
      border_rect, SkIntToScalar(2), SkIntToScalar(2), border_paint);

  SkRect inner_rect = {
    border_rect.fLeft + SkDoubleToScalar(0.5),
    border_rect.fTop + SkDoubleToScalar(0.5),
    border_rect.fRight - SkDoubleToScalar(0.5),
    border_rect.fBottom - SkDoubleToScalar(0.5),
  };

  SkPaint inner_paint;
  inner_paint.setAntiAlias(true);
  inner_paint.setARGB(0xFF, 0xF8, 0xF8, 0xF8);
  canvas->AsCanvasSkia()->drawRoundRect(
      inner_rect, SkIntToScalar(1.5), SkIntToScalar(1.5), inner_paint);
}

}  // namespace

namespace browser {

void ShowCreateWebAppShortcutsDialog(gfx::NativeWindow parent_window,
                                     TabContentsWrapper* tab_contents) {
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(),
      new CreateUrlApplicationShortcutView(tab_contents))->Show();
}

void ShowCreateChromeAppShortcutsDialog(gfx::NativeWindow parent_window,
                                        Profile* profile,
                                        const Extension* app) {
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(),
      new CreateChromeApplicationShortcutView(profile, app))->Show();
}

}  // namespace browser

class CreateUrlApplicationShortcutView::IconDownloadCallbackFunctor {
 public:
  explicit IconDownloadCallbackFunctor(CreateUrlApplicationShortcutView* owner)
      : owner_(owner) {
  }

  void Run(int download_id, bool errored, const SkBitmap& image) {
    if (owner_)
      owner_->OnIconDownloaded(errored, image);
    delete this;
  }

  void Cancel() {
    owner_ = NULL;
  }

 private:
  CreateUrlApplicationShortcutView* owner_;
};

CreateApplicationShortcutView::CreateApplicationShortcutView(Profile* profile)
    : profile_(profile) {}

CreateApplicationShortcutView::~CreateApplicationShortcutView() {}

void CreateApplicationShortcutView::InitControls() {
  // Create controls
  app_info_ = new AppInfoView(shortcut_info_.title, shortcut_info_.description,
                              shortcut_info_.favicon);
  create_shortcuts_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_LABEL)));
  create_shortcuts_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  desktop_check_box_ = AddCheckbox(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_DESKTOP_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kWebAppCreateOnDesktop));

  menu_check_box_ = NULL;
  quick_launch_check_box_ = NULL;

#if defined(OS_WIN)
  menu_check_box_ = AddCheckbox(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_START_MENU_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kWebAppCreateInAppsMenu));

  quick_launch_check_box_ = AddCheckbox(
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_PIN_TO_TASKBAR_CHKBOX)) :
        UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_CREATE_SHORTCUTS_QUICK_LAUNCH_BAR_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kWebAppCreateInQuickLaunchBar));
#elif defined(OS_LINUX)
  menu_check_box_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_MENU_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kWebAppCreateInAppsMenu));
#endif

  // Layout controls
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
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

  layout->AddPaddingRow(0, views::kPanelSubVerticalSpacing);
  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(create_shortcuts_label_);

  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);
  layout->StartRow(0, kTableColumnSetId);
  layout->AddView(desktop_check_box_);

  if (menu_check_box_ != NULL) {
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    layout->StartRow(0, kTableColumnSetId);
    layout->AddView(menu_check_box_);
  }

  if (quick_launch_check_box_ != NULL) {
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    layout->StartRow(0, kTableColumnSetId);
    layout->AddView(quick_launch_check_box_);
  }
}

gfx::Size CreateApplicationShortcutView::GetPreferredSize() {
  // TODO(evanm): should this use IDS_CREATE_SHORTCUTS_DIALOG_WIDTH_CHARS?
  static const int kDialogWidth = 360;
  int height = GetLayoutManager()->GetPreferredHeightForWidth(this,
      kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

std::wstring CreateApplicationShortcutView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_COMMIT));
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
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_TITLE));
}

bool CreateApplicationShortcutView::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK))
    return false;

  shortcut_info_.create_on_desktop = desktop_check_box_->checked();
  shortcut_info_.create_in_applications_menu = menu_check_box_ == NULL ? false :
      menu_check_box_->checked();

#if defined(OS_WIN)
  shortcut_info_.create_in_quick_launch_bar = quick_launch_check_box_ == NULL ?
      NULL : quick_launch_check_box_->checked();
#elif defined(OS_POSIX)
  // Create shortcut in Mac dock or as Linux (gnome/kde) application launcher
  // are not implemented yet.
  shortcut_info_.create_in_quick_launch_bar = false;
#endif

  web_app::CreateShortcut(profile_->GetPath(),
                          shortcut_info_,
                          NULL);
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

void CreateApplicationShortcutView::ButtonPressed(views::Button* sender,
                                                  const views::Event& event) {
  if (sender == desktop_check_box_)
    profile_->GetPrefs()->SetBoolean(prefs::kWebAppCreateOnDesktop,
        desktop_check_box_->checked() ? true : false);
  else if (sender == menu_check_box_)
    profile_->GetPrefs()->SetBoolean(prefs::kWebAppCreateInAppsMenu,
        menu_check_box_->checked() ? true : false);
  else if (sender == quick_launch_check_box_)
    profile_->GetPrefs()->SetBoolean(prefs::kWebAppCreateInQuickLaunchBar,
        quick_launch_check_box_->checked() ? true : false);

  // When no checkbox is checked we should not have the action button enabled.
  GetDialogClientView()->UpdateDialogButtons();
}

CreateUrlApplicationShortcutView::CreateUrlApplicationShortcutView(
    TabContentsWrapper* tab_contents)
    : CreateApplicationShortcutView(tab_contents->profile()),
      tab_contents_(tab_contents),
      pending_download_(NULL)  {

  web_app::GetShortcutInfoForTab(tab_contents_, &shortcut_info_);
  const WebApplicationInfo& app_info =
      tab_contents_->extension_tab_helper()->web_app_info();
  if (!app_info.icons.empty()) {
    web_app::GetIconsInfo(app_info, &unprocessed_icons_);
    FetchIcon();
  }

  InitControls();
}

CreateUrlApplicationShortcutView::~CreateUrlApplicationShortcutView() {
  if (pending_download_)
    pending_download_->Cancel();
}

bool CreateUrlApplicationShortcutView::Accept() {
  if (!CreateApplicationShortcutView::Accept())
    return false;

  tab_contents_->extension_tab_helper()->SetAppIcon(shortcut_info_.favicon);
  if (tab_contents_->tab_contents()->delegate()) {
    tab_contents_->tab_contents()->delegate()->ConvertContentsToApplication(
        tab_contents_->tab_contents());
  }
  return true;
}

void CreateUrlApplicationShortcutView::FetchIcon() {
  // There should only be fetch job at a time.
  DCHECK(pending_download_ == NULL);

  if (unprocessed_icons_.empty())  // No icons to fetch.
    return;

  pending_download_ = new IconDownloadCallbackFunctor(this);
  DCHECK(pending_download_);

  tab_contents_->tab_contents()->favicon_helper().DownloadImage(
      unprocessed_icons_.back().url,
      std::max(unprocessed_icons_.back().width,
               unprocessed_icons_.back().height),
      history::FAVICON,
      NewCallback(pending_download_, &IconDownloadCallbackFunctor::Run));

  unprocessed_icons_.pop_back();
}

void CreateUrlApplicationShortcutView::OnIconDownloaded(bool errored,
                                                        const SkBitmap& image) {
  pending_download_ = NULL;

  if (!errored && !image.isNull()) {
    shortcut_info_.favicon = image;
    static_cast<AppInfoView*>(app_info_)->UpdateIcon(shortcut_info_.favicon);
  } else {
    FetchIcon();
  }
}

CreateChromeApplicationShortcutView::CreateChromeApplicationShortcutView(
    Profile* profile,
    const Extension* app) :
      CreateApplicationShortcutView(profile),
      app_(app),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {

  shortcut_info_.extension_id = app_->id();
  shortcut_info_.url = GURL(app_->launch_web_url());
  shortcut_info_.title = UTF8ToUTF16(app_->name());
  shortcut_info_.description = UTF8ToUTF16(app_->description());

  // The icon will be resized to |max_size|.
  const gfx::Size max_size(kAppIconSize, kAppIconSize);

  // Look for an icon.  If there is no icon at the ideal size,
  // we will resize whatever we can get.  Making a large icon smaller
  // is prefered to making a small icon larger, so look for a larger
  // icon first:
  ExtensionResource icon_resource = app_->GetIconResource(
      kAppIconSize,
      ExtensionIconSet::MATCH_BIGGER);

  // If no icon exists that is the desired size or larger, get the
  // largest icon available:
  if (icon_resource.empty()) {
    icon_resource = app_->GetIconResource(
        kAppIconSize,
        ExtensionIconSet::MATCH_SMALLER);
  }

  tracker_.LoadImage(app_,
                     icon_resource,
                     max_size,
                     ImageLoadingTracker::DONT_CACHE);

  InitControls();
}

CreateChromeApplicationShortcutView::~CreateChromeApplicationShortcutView() {}

// Called by tracker_ when the app's icon is loaded.
void CreateChromeApplicationShortcutView::OnImageLoaded(
    SkBitmap* image, const ExtensionResource& resource, int index) {
  if (image->isNull()) {
    NOTREACHED() << "Corrupt image in profile?";
    return;
  }
  shortcut_info_.favicon = *image;
  static_cast<AppInfoView*>(app_info_)->UpdateIcon(shortcut_info_.favicon);
}

