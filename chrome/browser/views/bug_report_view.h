// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BUG_REPORT_VIEW_H_
#define CHROME_BROWSER_VIEWS_BUG_REPORT_VIEW_H_

#include "chrome/common/net/url_fetcher.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/link.h"
#include "views/controls/image_view.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/syslogs_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#endif

namespace views {
class Checkbox;
class Label;
class Throbber;
class Window;
class RadioButton;
class Link;
}

class Profile;
class TabContents;
class BugReportComboBoxModel;

// BugReportView draws the dialog that allows the user to report a
// bug in rendering a particular page (note: this is not a crash
// report, which are handled separately by Breakpad).  It packages
// up the URL, a text description, system information and optionally
// a screenshot; then it submits the info through https to the google
// feedback chrome end-point.
//
// Note: This UI is being used for the Chrome OS dogfood release only
//       In the very next iteration, this will be replaced by a HTML
//       based UI, which will be common for all platforms
class BugReportView : public views::View,
                      public views::DialogDelegate,
                      public views::Combobox::Listener,
#if defined(OS_CHROMEOS)
                      public views::LinkController,
#endif
                      public views::Textfield::Controller {
 public:
  BugReportView(Profile* profile, TabContents* tab);
  virtual ~BugReportView();

  // NOTE: set_captured_image takes ownership of the vector
  void set_captured_image(std::vector<unsigned char>* png_data) {
    captured_image_.reset(png_data);
  }

  void set_screen_size(const gfx::Rect& screen_size) {
    screen_size_ = screen_size;
  }

  // Set all additional reporting controls to disabled
  // if phishing report
  void UpdateReportingControls(bool is_phishing_report);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // views::Textfield::Controller implementation:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& key);

  // views::Combobox::Listener implementation:
  virtual void ItemChanged(views::Combobox* combobox, int prev_index,
                           int new_index);

#if defined(OS_CHROMEOS)
  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Disable the include last image radio control
  void DisableLastImageRadio() {
    include_last_screen_image_radio_->SetEnabled(false);
  }

  // NOTE: set_last_image takes ownership of the vector
  void set_last_image(std::vector<unsigned char>* png_data) {
    last_image_.reset(png_data);
  }
#endif

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDefaultDialogButton() const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();

 private:
  class PostCleanup;

  // Set OS Version information in a string (maj.minor.build SP)
  void SetOSVersion(std::string *os_version);

  // Initializes the controls on the dialog.
  void SetupControl();
  // helper function to create a MIME part boundary string
  void CreateMimeBoundary(std::string *out);
  // Sends the data via an HTTP POST
  void SendReport();

  // Redirects the user to Google's phishing reporting page.
  void ReportPhishing();

  views::Label* bug_type_label_;
  views::Combobox* bug_type_combo_;
  views::Label* page_title_label_;
  views::Label* page_title_text_;
  views::Label* page_url_label_;
  views::Textfield* page_url_text_;
  views::Label* description_label_;
  views::Textfield* description_text_;
  views::Checkbox* include_page_source_checkbox_;
#if defined(OS_CHROMEOS)
  views::Label* user_email_label_;
  views::Textfield* user_email_text_;
  views::RadioButton* include_new_screen_image_radio_;
  views::RadioButton* include_last_screen_image_radio_;
  views::RadioButton* include_no_screen_image_radio_;
  views::Link* system_information_url_control_;

  scoped_ptr<chromeos::LogDictionaryType> sys_info_;
  scoped_ptr< std::vector<unsigned char> > last_image_;
#endif
  views::Checkbox* include_page_image_checkbox_;


  scoped_ptr<BugReportComboBoxModel> bug_type_model_;

  Profile* profile_;

  std::wstring version_;
  gfx::Rect screen_size_;
  scoped_ptr< std::vector<unsigned char> > captured_image_;

  TabContents* tab_;

  // Used to distinguish the report type: Phishing or other.
  int problem_type_;

  // Save the description the user types in when we clear the dialog for the
  // phishing option. If the user changes the report type back, we reinstate
  // their original text so they don't have to type it again.
  std::wstring old_report_text_;

  DISALLOW_COPY_AND_ASSIGN(BugReportView);
};

#endif  // CHROME_BROWSER_VIEWS_BUG_REPORT_VIEW_H_
