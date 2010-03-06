// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/account_creation_view.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace {

const char kCreateAccountPageUrl[] =
    "https://www.google.com/accounts/NewAccount?btmpl=tier2_mobile";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, public:
AccountScreen::AccountScreen(WizardScreenDelegate* delegate)
    : ViewScreen<AccountCreationView>(delegate),
      inserted_css_(false) {
}

AccountScreen::~AccountScreen() {
}

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, ViewScreen implementation:
void AccountScreen::InitView() {
  ViewScreen<AccountCreationView>::InitView();

  GURL url(kCreateAccountPageUrl);
  Profile* profile = ProfileManager::GetLoginWizardProfile();
  view()->InitDOM(profile,
                  SiteInstance::CreateSiteInstanceForURL(profile, url));
  view()->SetTabContentsDelegate(this);
  view()->LoadURL(url);
}

AccountCreationView* AccountScreen::CreateView() {
  return new AccountCreationView();
}

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, TabContentsDelegate implementation:
void AccountScreen::NavigationStateChanged(const TabContents* source,
                                           unsigned changed_flags) {
  LOG(INFO) << "NavigationStateChanged: " << changed_flags << '\n';
  if (!inserted_css_ && source->render_view_host()) {
    source->render_view_host()->InsertCSSInWebFrame(
        L"",
        "body > img, hr, br, a, li, font, pre, b {"
        " display: none; "
        "} "
        "#smhck {"
        " display: none; "
        "} "
        "input + br {"
        " display: inline; "
        "} "
        ".label {"
        " width: 250px;"
        " display: inline-block; "
        "} "
        "br + #submitbutton {"
        " display: inline; "
        "} ",
        "");
    source->render_view_host()->ExecuteJavascriptInWebFrame(
        L"",
        L"var smhck = document.getElementById('smhck');\n"
        L"if (smhck.checked) {"
        L"  smhck.checked = false;\n"
        L"  smhck.value = 0;\n"
        L"  var x=document.getElementById('createaccount');\n"
        L"  for (var i=0;i<x.childNodes.length-4;i++) {\n"
        L"    if (x.childNodes[i].nodeName == '#text' &&\n"
        L"        x.childNodes[i+1].nodeName == 'BR' &&\n"
        L"        x.childNodes[i+2].nodeName == '#text' &&\n"
        L"        x.childNodes[i+3].nodeName == 'INPUT' &&\n"
        L"        x.childNodes[i+3].type != 'hidden' &&\n"
        L"        x.childNodes[i+3].type != 'checkbox' &&\n"
        L"        x.childNodes[i+3].type != 'submit') {\n"
        L"      var s = document.createElement('span');\n"
        L"      s.innerText = x.childNodes[i].textContent;\n"
        L"      s.className = 'label';\n"
        L"      x.replaceChild(s, x.childNodes[i]);\n"
        L"    } else if (x.childNodes[i].nodeName == '#text') {\n"
        L"      x.childNodes[i].textContent = '';\n"
        L"    }\n"
        L"  }\n"
        L"}\n");
  }
}
