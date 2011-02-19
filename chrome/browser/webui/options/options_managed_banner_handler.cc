// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/options_managed_banner_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webui/web_ui.h"

OptionsManagedBannerHandler::OptionsManagedBannerHandler(
    WebUI* web_ui, const string16& page_name, OptionsPage page)
    : policy::ManagedPrefsBannerBase(web_ui->GetProfile()->GetPrefs(), page),
      web_ui_(web_ui), page_name_(page_name), page_(page) {
  // Initialize the visibility state of the banner.
  SetupBannerVisibilty();
}

OptionsManagedBannerHandler::~OptionsManagedBannerHandler() {}

void OptionsManagedBannerHandler::OnUpdateVisibility() {
  // A preference that may be managed has changed.  Update our visibility
  // state.
  SetupBannerVisibilty();
}

void OptionsManagedBannerHandler::SetupBannerVisibilty() {
  // Construct the banner visibility script name.
  string16 script = ASCIIToUTF16("options.") + page_name_ +
      ASCIIToUTF16(".getInstance().setManagedBannerVisibility");

  // Get the visiblity value from the base class.
  FundamentalValue visibility(DetermineVisibility());

  // Set the managed state in the javascript handler.
  web_ui_->CallJavascriptFunction(UTF16ToWideHack(script), visibility);
}
