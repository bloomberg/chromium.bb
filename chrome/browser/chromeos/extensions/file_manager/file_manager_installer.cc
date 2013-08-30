// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_installer.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace file_manager {

namespace {
const char kWebContentsDestroyedError[] = "WebContents is destroyed.";
}  // namespace

class FileManagerInstaller::WebContentsObserver
    : public content::WebContentsObserver {

 public:
  explicit WebContentsObserver(
      content::WebContents* web_contents,
      FileManagerInstaller* parent)
      : content::WebContentsObserver(web_contents),
        parent_(parent) {
  }

 protected:
  // content::WebContentsObserver implementation.
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE {
    parent_->OnWebContentsDestroyed(web_contents);
  }

 private:
  FileManagerInstaller* parent_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebContentsObserver);
};

FileManagerInstaller::FileManagerInstaller(
    content::WebContents* web_contents,
    const std::string& webstore_item_id,
    Profile* profile,
    const Callback& callback)
    : extensions::WebstoreStandaloneInstaller(
          webstore_item_id,
          profile,
          callback),
      callback_(callback),
      web_contents_(web_contents),
      web_contents_observer_(new WebContentsObserver(web_contents, this)) {
}

FileManagerInstaller::~FileManagerInstaller() {}

bool FileManagerInstaller::CheckRequestorAlive() const {
  // The tab may have gone away - cancel installation in that case.
  return web_contents_ != NULL;
}

const GURL& FileManagerInstaller::GetRequestorURL() const {
  return GURL::EmptyGURL();
}

scoped_ptr<ExtensionInstallPrompt::Prompt>
FileManagerInstaller::CreateInstallPrompt() const {
  scoped_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::INLINE_INSTALL_PROMPT));

  prompt->SetInlineInstallWebstoreData(localized_user_count(),
                                       show_user_count(),
                                       average_rating(),
                                       rating_count());
  return prompt.Pass();
}

bool FileManagerInstaller::ShouldShowPostInstallUI() const {
  return false;
}

bool FileManagerInstaller::ShouldShowAppInstalledBubble() const {
  return false;
}

content::WebContents* FileManagerInstaller::GetWebContents() const {
  return web_contents_;
}

bool FileManagerInstaller::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  DCHECK(error != NULL);
  DCHECK(error->empty());
  return true;
}

bool FileManagerInstaller::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  DCHECK(error != NULL);
  DCHECK(error->empty());
  return true;
}

void FileManagerInstaller::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  callback_.Run(false, kWebContentsDestroyedError);
  AbortInstall();
}

}  // namespace file_manager
