// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bug_report_view.h"

#include <string>
#include <vector>

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/waitable_event.h"
#include "chrome/browser/bug_report_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "unicode/locid.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
#include "app/x11_util.h"
#else
#include "app/win_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using views::ColumnSet;
using views::GridLayout;

// Report a bug data version.
static const int kBugReportVersion = 1;
static const int kScreenImageRadioGroup = 2;
static const char kScreenshotsRelativePath[] = "/Screenshots";
static const char kScreenshotPattern[] = "*.png";

// Number of lines description field can display at one time.
static const int kDescriptionLines = 5;

class BugReportComboBoxModel : public ComboboxModel {
 public:
  BugReportComboBoxModel() {}

  // ComboboxModel interface.
  virtual int GetItemCount() {
    return BugReportUtil::OTHER_PROBLEM + 1;
  }

  virtual std::wstring GetItemAt(int index) {
    return GetItemAtIndex(index);
  }

  static std::wstring GetItemAtIndex(int index) {
#if defined(OS_CHROMEOS)
    switch (index) {
      case BugReportUtil::CONNECTIVITY_ISSUE:
        return l10n_util::GetString(IDS_BUGREPORT_CONNECTIVITY_ISSUE);
      case BugReportUtil::SYNC_ISSUE:
        return l10n_util::GetString(IDS_BUGREPORT_SYNC_ISSUE);
      case BugReportUtil::CRASH_ISSUE:
        return l10n_util::GetString(IDS_BUGREPORT_CRASH_ISSUE);
      case BugReportUtil::PAGE_FORMATTING:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_FORMATTING);
      case BugReportUtil::EXTENSION_ISSUE:
        return l10n_util::GetString(IDS_BUGREPORT_EXTENSION_ISSUE);
      case BugReportUtil::SUSPEND_ISSUE:
        return l10n_util::GetString(IDS_BUGREPORT_SUSPEND_ISSUE);
      case BugReportUtil::PHISHING_PAGE:
        return l10n_util::GetString(IDS_BUGREPORT_PHISHING_PAGE);
      case BugReportUtil::OTHER_PROBLEM:
        return l10n_util::GetString(IDS_BUGREPORT_OTHER_PROBLEM);
      default:
        NOTREACHED();
        return std::wstring();
    }
#else
    switch (index) {
      case BugReportUtil::PAGE_WONT_LOAD:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_WONT_LOAD);
      case BugReportUtil::PAGE_LOOKS_ODD:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_LOOKS_ODD);
      case BugReportUtil::PHISHING_PAGE:
        return l10n_util::GetString(IDS_BUGREPORT_PHISHING_PAGE);
      case BugReportUtil::CANT_SIGN_IN:
        return l10n_util::GetString(IDS_BUGREPORT_CANT_SIGN_IN);
      case BugReportUtil::CHROME_MISBEHAVES:
        return l10n_util::GetString(IDS_BUGREPORT_CHROME_MISBEHAVES);
      case BugReportUtil::SOMETHING_MISSING:
        return l10n_util::GetString(IDS_BUGREPORT_SOMETHING_MISSING);
      case BugReportUtil::BROWSER_CRASH:
        return l10n_util::GetString(IDS_BUGREPORT_BROWSER_CRASH);
      case BugReportUtil::OTHER_PROBLEM:
        return l10n_util::GetString(IDS_BUGREPORT_OTHER_PROBLEM);
      default:
        NOTREACHED();
        return std::wstring();
    }
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BugReportComboBoxModel);
};

namespace {

#if defined(OS_CHROMEOS)
class LastScreenshotTask : public Task {
 public:
  LastScreenshotTask(std::string* image_str,
                     base::WaitableEvent* task_waitable)
      : image_str_(image_str),
        task_waitable_(task_waitable) {
  }

 private:
  void Run() {
    FilePath fileshelf_path;
    // TODO(rkc): Change this to use FilePath.Append() once the cros
    // issue with with it is fixed
    if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                          &fileshelf_path)) {
      *image_str_ = "";
      task_waitable_->Signal();
    }

    FilePath screenshots_path(fileshelf_path.value() +
                              std::string(kScreenshotsRelativePath));
    file_util::FileEnumerator screenshots(screenshots_path, false,
                                          file_util::FileEnumerator::FILES,
                                          std::string(kScreenshotPattern));
    FilePath screenshot = screenshots.Next();
    FilePath latest("");
    time_t last_mtime = 0;
    while (!screenshot.empty()) {
      file_util::FileEnumerator::FindInfo info;
      screenshots.GetFindInfo(&info);
      if (info.stat.st_mtime > last_mtime) {
        last_mtime = info.stat.st_mtime;
        latest = screenshot;
      }
      screenshot = screenshots.Next();
    }

    if (!file_util::ReadFileToString(latest, image_str_))
      *image_str_ = "";
    task_waitable_->Signal();
  }
 private:
  std::string* image_str_;
  base::WaitableEvent* task_waitable_;
};
#endif

bool GetLastScreenshot(std::string* image_str) {
#if defined(OS_CHROMEOS)
  base::WaitableEvent task_waitable(true, false);
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                         new LastScreenshotTask(image_str, &task_waitable));
  task_waitable.Wait();
  if (*image_str == "")
    return false;
  else
    return true;
#else
  return false;
#endif
}

}  // namespace

namespace browser {

// Global "display this dialog" function declared in browser_dialogs.h.
void ShowBugReportView(views::Window* parent,
                       Profile* profile,
                       TabContents* tab) {
  BugReportView* view = new BugReportView(profile, tab);

  // Get the size of the parent window to capture screenshot dimensions
  gfx::Rect screen_size = parent->GetBounds();
  view->set_screen_size(screen_size);

  // Grab an exact snapshot of the window that the user is seeing (i.e. as
  // rendered--do not re-render, and include windowed plugins).
  std::vector<unsigned char> *screenshot_png = new std::vector<unsigned char>;

#if defined(OS_LINUX)
  x11_util::GrabWindowSnapshot(parent->GetNativeWindow(), screenshot_png);
#else
  win_util::GrabWindowSnapshot(parent->GetNativeWindow(), screenshot_png);
#endif

  // The BugReportView takes ownership of the image data, and will dispose of
  // it in its destructor
  view->set_captured_image(screenshot_png);

#if defined(OS_CHROMEOS)
  // Get last screenshot taken
  std::string image_str;
  bool have_last_image = false;
  if (GetLastScreenshot(&image_str)) {
    // reuse screenshot_png; previous pointer now owned by BugReportView
    screenshot_png = new std::vector<unsigned char>(image_str.begin(),
                                                    image_str.end());
    have_last_image = true;
  } else {
    // else set it to be an empty vector
    screenshot_png = new std::vector<unsigned char>;
  }
  view->set_last_image(screenshot_png);

  // Create and show the dialog
  views::Window::CreateChromeWindow(parent->GetNativeWindow(), gfx::Rect(),
                                    view)->Show();
  if (!have_last_image)
    view->DisableLastImageRadio();
#endif
}

}  // namespace browser

// BugReportView - create and submit a bug report from the user.
// This is separate from crash reporting, which is handled by Breakpad.
BugReportView::BugReportView(Profile* profile, TabContents* tab)
    : include_page_source_checkbox_(NULL),
      include_page_image_checkbox_(NULL),
      profile_(profile),
      tab_(tab),
      problem_type_(0) {
  DCHECK(profile);
  SetupControl();

  // We want to use the URL of the current committed entry (the current URL may
  // actually be the pending one).
  if (tab->controller().GetActiveEntry()) {
    page_url_text_->SetText(UTF8ToUTF16(
        tab->controller().GetActiveEntry()->url().spec()));
  }

#if defined(OS_CHROMEOS)
  // Get and set the gaia e-mail
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager) {
    user_email_text_->SetText(UTF8ToUTF16(std::string("")));
  } else {
    const std::string& email = manager->logged_in_user().email();
    user_email_text_->SetText(UTF8ToUTF16(email));
    if (!email.empty())
      user_email_text_->SetEnabled(false);
  }
#endif

  // Retrieve the application version info.
  scoped_ptr<FileVersionInfo> version_info(chrome::GetChromeVersionInfo());
  if (version_info.get()) {
    version_ = version_info->product_name() + L" - " +
        version_info->file_version() +
        L" (" + version_info->last_change() + L")";
  }


  FilePath tmpfilename;

#if defined(OS_CHROMEOS)
  chromeos::SyslogsLibrary* syslogs_lib =
      chromeos::CrosLibrary::Get()->GetSyslogsLibrary();
  if (syslogs_lib) {
    sys_info_.reset(syslogs_lib->GetSyslogs(&tmpfilename));
  }
#endif
}

BugReportView::~BugReportView() {
}

void BugReportView::SetupControl() {
  bug_type_model_.reset(new BugReportComboBoxModel);

  // Adds all controls.
  bug_type_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_BUG_TYPE));
  bug_type_combo_ = new views::Combobox(bug_type_model_.get());
  bug_type_combo_->set_listener(this);
  bug_type_combo_->SetAccessibleName(bug_type_label_->GetText());

  page_title_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_PAGE_TITLE));
  page_title_text_ = new views::Label(UTF16ToWideHack(tab_->GetTitle()));
  page_url_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_URL_LABEL));
  // page_url_text_'s text (if any) is filled in after dialog creation.
  page_url_text_ = new views::Textfield;
  page_url_text_->SetController(this);
  page_url_text_->SetAccessibleName(page_url_label_->GetText());

#if defined(OS_CHROMEOS)
  user_email_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_USER_EMAIL_LABEL));
  // user_email_text_'s text (if any) is filled in after dialog creation.
  user_email_text_ = new views::Textfield;
  user_email_text_->SetController(this);
  user_email_text_->SetAccessibleName(user_email_label_->GetText());
#endif

  description_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_DESCRIPTION_LABEL));
  description_text_ =
      new views::Textfield(views::Textfield::STYLE_MULTILINE);
  description_text_->SetHeightInLines(kDescriptionLines);
  description_text_->SetAccessibleName(description_label_->GetText());

  include_page_source_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_SOURCE_CHKBOX));
  include_page_source_checkbox_->SetChecked(true);

#if defined(OS_CHROMEOS)
  include_last_screen_image_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_LAST_SCREEN_IMAGE),
      kScreenImageRadioGroup);

  include_new_screen_image_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_NEW_SCREEN_IMAGE),
      kScreenImageRadioGroup);

  include_no_screen_image_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_NO_SCREEN_IMAGE),
      kScreenImageRadioGroup);

  system_information_url_control_ = new views::Link(
      l10n_util::GetString(IDS_BUGREPORT_SYSTEM_INFORMATION_URL_TEXT));
  system_information_url_control_->SetController(this);

  include_new_screen_image_radio_->SetChecked(true);
#endif
  include_page_image_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_IMAGE_CHKBOX));
  include_page_image_checkbox_->SetChecked(true);

  // Arranges controls by using GridLayout.
  const int column_set_id = 0;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing * 2);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Page Title and text.
  layout->StartRow(0, column_set_id);
  layout->AddView(page_title_label_);
  layout->AddView(page_title_text_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Bug type and combo box.
  layout->StartRow(0, column_set_id);
  layout->AddView(bug_type_label_, 1, 1, GridLayout::LEADING, GridLayout::FILL);
  layout->AddView(bug_type_combo_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Page URL and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(page_url_label_);
  layout->AddView(page_url_text_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Description label and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(description_label_, 1, 1, GridLayout::LEADING,
                  GridLayout::LEADING);
  layout->AddView(description_text_, 1, 1, GridLayout::FILL,
                  GridLayout::LEADING);
#if defined(OS_CHROMEOS)
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  // Page URL and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(user_email_label_);
  layout->AddView(user_email_text_);
#endif
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // Checkboxes.
  // The include page source checkbox is hidden until we can make it work.
  // layout->StartRow(0, column_set_id);
  // layout->SkipColumns(1);
  // layout->AddView(include_page_source_checkbox_);
  // layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->SkipColumns(1);
#if defined(OS_CHROMEOS)
  // Radio boxes to select last screen shot or,

  // new screenshot
  layout->AddView(include_new_screen_image_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  // last screenshot taken
  layout->StartRow(0, column_set_id);
  layout->SkipColumns(1);
  layout->AddView(include_last_screen_image_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  // no screenshot
  layout->StartRow(0, column_set_id);
  layout->SkipColumns(1);
  layout->AddView(include_no_screen_image_radio_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, column_set_id);
  layout->SkipColumns(1);
  layout->AddView(system_information_url_control_, 1, 1, GridLayout::LEADING,
                  GridLayout::CENTER);
#else
  if (include_page_image_checkbox_) {
    layout->StartRow(0, column_set_id);
    layout->SkipColumns(1);
    layout->AddView(include_page_image_checkbox_);
  }
#endif

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
}

gfx::Size BugReportView::GetPreferredSize() {
  gfx::Size size = views::Window::GetLocalizedContentsSize(
      IDS_BUGREPORT_DIALOG_WIDTH_CHARS,
#if defined(OS_CHROMEOS)
      IDS_CHROMEOS_BUGREPORT_DIALOG_HEIGHT_LINES);
#else
      IDS_BUGREPORT_DIALOG_HEIGHT_LINES);
#endif
  return size;
}

void BugReportView::UpdateReportingControls(bool is_phishing_report) {
  // page source, screen/page images, system information
  // are not needed if it's a phishing report

  include_page_source_checkbox_->SetEnabled(!is_phishing_report);
  include_page_source_checkbox_->SetChecked(!is_phishing_report);

#if defined(OS_CHROMEOS)
  include_new_screen_image_radio_->SetEnabled(!is_phishing_report);
  if (!last_image_->empty())
    include_last_screen_image_radio_->SetEnabled(!is_phishing_report);
  include_no_screen_image_radio_->SetEnabled(!is_phishing_report);
#else
  if (include_page_image_checkbox_) {
    include_page_image_checkbox_->SetEnabled(!is_phishing_report);
    include_page_image_checkbox_->SetChecked(!is_phishing_report);
  }
#endif
}

void BugReportView::ItemChanged(views::Combobox* combobox,
                                int prev_index,
                                int new_index) {
  if (new_index == prev_index)
    return;

  problem_type_ = new_index;
  bool is_phishing_report = new_index == BugReportUtil::PHISHING_PAGE;

  description_text_->SetEnabled(!is_phishing_report);
  description_text_->SetReadOnly(is_phishing_report);
  if (is_phishing_report) {
    old_report_text_ = UTF16ToWide(description_text_->text());
    description_text_->SetText(string16());
  } else if (!old_report_text_.empty()) {
    description_text_->SetText(WideToUTF16Hack(old_report_text_));
    old_report_text_.clear();
  }

  UpdateReportingControls(is_phishing_report);
  GetDialogClientView()->UpdateDialogButtons();
}

void BugReportView::ContentsChanged(views::Textfield* sender,
                                    const string16& new_contents) {
}

bool BugReportView::HandleKeystroke(views::Textfield* sender,
                                    const views::Textfield::Keystroke& key) {
  return false;
}

std::wstring BugReportView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    if (problem_type_ == BugReportUtil::PHISHING_PAGE)
      return l10n_util::GetString(IDS_BUGREPORT_SEND_PHISHING_REPORT);
    else
      return l10n_util::GetString(IDS_BUGREPORT_SEND_REPORT);
  } else {
    return std::wstring();
  }
}

int BugReportView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

bool BugReportView::CanResize() const {
  return false;
}

bool BugReportView::CanMaximize() const {
  return false;
}

bool BugReportView::IsAlwaysOnTop() const {
  return false;
}

bool BugReportView::HasAlwaysOnTopMenu() const {
  return false;
}

bool BugReportView::IsModal() const {
  return true;
}

std::wstring BugReportView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BUGREPORT_TITLE);
}


bool BugReportView::Accept() {
  if (IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    if (problem_type_ == BugReportUtil::PHISHING_PAGE) {
      BugReportUtil::ReportPhishing(tab_,
          UTF16ToUTF8(page_url_text_->text()));
    } else {
      char* image_data = NULL;
      size_t image_data_size = 0;
#if defined(OS_CHROMEOS)
      if (include_new_screen_image_radio_->checked() &&
          !captured_image_->empty()) {
        image_data = reinterpret_cast<char *>(&captured_image_->front());
        image_data_size = captured_image_->size();
      } else if (include_last_screen_image_radio_->checked() &&
                 !last_image_->empty()) {
        image_data = reinterpret_cast<char *>(&last_image_->front());
        image_data_size = last_image_->size();
      }
#else
      if (include_page_image_checkbox_->checked() && captured_image_.get() &&
          !captured_image_->empty()) {
        image_data = reinterpret_cast<char *>(&captured_image_->front());
        image_data_size = captured_image_->size();
      }
#endif
#if defined(OS_CHROMEOS)
      BugReportUtil::SendReport(profile_,
          WideToUTF8(page_title_text_->GetText()),
          problem_type_,
          UTF16ToUTF8(page_url_text_->text()),
          UTF16ToUTF8(user_email_text_->text()),
          UTF16ToUTF8(description_text_->text()),
          image_data, image_data_size,
          screen_size_.width(), screen_size_.height(),
          WideToUTF8(bug_type_combo_->model()->GetItemAt(problem_type_)),
          sys_info_.get());
#else
      BugReportUtil::SendReport(profile_,
          WideToUTF8(page_title_text_->GetText()),
          problem_type_,
          UTF16ToUTF8(page_url_text_->text()),
          std::string(),
          UTF16ToUTF8(description_text_->text()),
          image_data, image_data_size,
          screen_size_.width(), screen_size_.height());
#endif
    }
  }
  return true;
}

#if defined(OS_CHROMEOS)
void BugReportView::LinkActivated(views::Link* source,
                                    int event_flags) {
  GURL url;
  if (source == system_information_url_control_) {
    url = GURL(chrome::kAboutSystemURL);
  } else {
    NOTREACHED() << "Unknown link source";
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  if (browser)
    browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
}
#endif


views::View* BugReportView::GetContentsView() {
  return this;
}
