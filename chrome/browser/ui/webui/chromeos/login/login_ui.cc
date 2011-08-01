// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/login_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/login_ui_helpers.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace chromeos {

// Boilerplate class that is used to associate the LoginUI code with the URL
// "chrome://login"
class LoginUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit LoginUIHTMLSource(base::DictionaryValue* localized_strings);
  virtual ~LoginUIHTMLSource();

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  scoped_ptr<HTMLOperationsInterface> html_operations_;
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIHTMLSource);
};

// LoginUIHTMLSource, public: --------------------------------------------------

LoginUIHTMLSource::LoginUIHTMLSource(base::DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUILoginHost, MessageLoop::current()),
      html_operations_(new HTMLOperationsInterface()),
      localized_strings_(localized_strings) {
}

LoginUIHTMLSource::~LoginUIHTMLSource() {
}

void LoginUIHTMLSource::StartDataRequest(const std::string& path,
                                         bool is_incognito,
                                         int request_id) {
  SetFontAndTextDirection(localized_strings_.get());

  base::StringPiece login_html = html_operations_->GetLoginHTML();
  std::string full_html = html_operations_->GetFullHTML(
      login_html, localized_strings_.get());

  SendResponse(request_id, base::RefCountedString::TakeString(&full_html));
}

std::string LoginUIHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
}

// LoginUI, public: ------------------------------------------------------------

LoginUI::LoginUI(TabContents* contents)
    : ChromeWebUI(contents) {
  scoped_ptr<base::DictionaryValue> localized_strings(
      new base::DictionaryValue);

  BaseScreenHandler* signin_screen_handler = new SigninScreenHandler;
  AddMessageHandler(signin_screen_handler->Attach(this));
  signin_screen_handler->GetLocalizedStrings(localized_strings.get());

  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  LoginUIHTMLSource* html_source =
      new LoginUIHTMLSource(localized_strings.release());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);

  // Load the theme URLs.
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);

  // Load the user-image URLs
  // Set up the chrome://userimage/ source.
  chromeos::UserImageSource* user_image_source =
      new chromeos::UserImageSource();
  profile->GetChromeURLDataManager()->AddDataSource(user_image_source);
}

}  // namespace chromeos
