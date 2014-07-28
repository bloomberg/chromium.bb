// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_install_with_prompt.h"

#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

WebstoreInstallWithPrompt::WebstoreInstallWithPrompt(
    const std::string& webstore_item_id,
    Profile* profile,
    const Callback& callback)
    : WebstoreStandaloneInstaller(webstore_item_id, profile, callback),
      show_post_install_ui_(true),
      dummy_web_contents_(
          WebContents::Create(WebContents::CreateParams(profile))),
      parent_window_(NULL) {
  set_install_source(WebstoreInstaller::INSTALL_SOURCE_OTHER);
}

WebstoreInstallWithPrompt::WebstoreInstallWithPrompt(
    const std::string& webstore_item_id,
    Profile* profile,
    gfx::NativeWindow parent_window,
    const Callback& callback)
    : WebstoreStandaloneInstaller(webstore_item_id, profile, callback),
      show_post_install_ui_(true),
      dummy_web_contents_(
          WebContents::Create(WebContents::CreateParams(profile))),
      parent_window_(parent_window) {
  DCHECK(parent_window);
  set_install_source(WebstoreInstaller::INSTALL_SOURCE_OTHER);
}

WebstoreInstallWithPrompt::~WebstoreInstallWithPrompt() {
}

bool WebstoreInstallWithPrompt::CheckRequestorAlive() const {
  // Assume the requestor is always alive.
  return true;
}

const GURL& WebstoreInstallWithPrompt::GetRequestorURL() const {
  return dummy_requestor_url_;
}

scoped_refptr<ExtensionInstallPrompt::Prompt>
WebstoreInstallWithPrompt::CreateInstallPrompt() const {
  return new ExtensionInstallPrompt::Prompt(
      ExtensionInstallPrompt::INSTALL_PROMPT);
}

scoped_ptr<ExtensionInstallPrompt>
WebstoreInstallWithPrompt::CreateInstallUI() {
  // Create an ExtensionInstallPrompt. If the parent window is NULL, the dialog
  // will be placed in the middle of the screen.
  return make_scoped_ptr(
      new ExtensionInstallPrompt(profile(), parent_window_, this));
}

bool WebstoreInstallWithPrompt::ShouldShowPostInstallUI() const {
  return show_post_install_ui_;
}

bool WebstoreInstallWithPrompt::ShouldShowAppInstalledBubble() const {
  return false;
}

WebContents* WebstoreInstallWithPrompt::GetWebContents() const {
  return dummy_web_contents_.get();
}

bool WebstoreInstallWithPrompt::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  // Assume the requestor is trusted.
  *error = std::string();
  return true;
}

bool WebstoreInstallWithPrompt::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  // Assume the requestor is trusted.
  *error = std::string();
  return true;
}

content::WebContents* WebstoreInstallWithPrompt::OpenURL(
    const content::OpenURLParams& params) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile(),
                                                 chrome::GetActiveDesktop());
  return displayer.browser()->OpenURL(params);
}

}  // namespace extensions
