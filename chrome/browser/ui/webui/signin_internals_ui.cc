// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_internals_ui.h"

#include "base/hash.h"
#include "base/memory/ref_counted.h"
#include "base/tracked_objects.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/signin_internals_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

ChromeWebUIDataSource* CreateSignInInternalsHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUISignInInternalsHost);

  source->set_json_path("strings.js");
  source->add_resource_path("signin_internals.js",
                            IDR_SIGNIN_INTERNALS_INDEX_JS);
  source->set_default_resource(IDR_SIGNIN_INTERNALS_INDEX_HTML);
  return source;
}

} //  namespace

SignInInternalsUI::SignInInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile,
                                      CreateSignInInternalsHTMLSource());
  SigninManagerFactory::GetForProfile(profile)->
      about_signin_internals()->AddSigninObserver(this);
}

SignInInternalsUI::~SignInInternalsUI() {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerFactory::GetForProfile(profile)->
      about_signin_internals()->RemoveSigninObserver(this);
}

bool SignInInternalsUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                                   const std::string& name,
                                                   const ListValue& content) {
  return false;
}

void SignInInternalsUI::OnSigninStateChanged(const DictionaryValue* info) {}
