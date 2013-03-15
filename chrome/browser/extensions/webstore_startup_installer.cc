// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_startup_installer.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

WebstoreStartupInstaller::WebstoreStartupInstaller(
    const std::string& webstore_item_id,
    Profile* profile,
    bool show_prompt,
    const Callback& callback)
    : WebstoreStandaloneInstaller(
          webstore_item_id,
          profile,
          callback),
      show_prompt_(show_prompt),
      dummy_web_contents_(
          WebContents::Create(WebContents::CreateParams(profile))) {
}

WebstoreStartupInstaller::~WebstoreStartupInstaller() {}

bool WebstoreStartupInstaller::CheckRequestorAlive() const {
  // Requestor is the command line, so it's always alive.
  return true;
}

const GURL& WebstoreStartupInstaller::GetRequestorURL() const {
  return dummy_requestor_url_;
}

scoped_ptr<ExtensionInstallPrompt::Prompt>
WebstoreStartupInstaller::CreateInstallPrompt() const {
  scoped_ptr<ExtensionInstallPrompt::Prompt> prompt;
  if (show_prompt_)
    prompt.reset(new ExtensionInstallPrompt::Prompt(
        ExtensionInstallPrompt::INSTALL_PROMPT));
  return prompt.Pass();
}

bool WebstoreStartupInstaller::ShouldShowPostInstallUI() const {
  return false;
}

bool WebstoreStartupInstaller::ShouldShowAppInstalledBubble() const {
  return false;
}

WebContents* WebstoreStartupInstaller::GetWebContents() const {
  return dummy_web_contents_.get();
}

bool WebstoreStartupInstaller::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  // Requestor is the command line: ignore the property set in the store
  // and always permit inline installs.
  *error = "";
  return true;
}

bool WebstoreStartupInstaller::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  // Requestor is the command line: always treat it as trusted.
  *error = "";
  return true;
}


}  // namespace extensions
