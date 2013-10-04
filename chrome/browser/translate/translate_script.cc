// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_script.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/translate/translate_url_fetcher.h"
#include "chrome/browser/translate/translate_url_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/translate/common/translate_util.h"
#include "google_apis/google_api_keys.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const int kExpirationDelayDays = 1;

}  // namespace

const char TranslateScript::kScriptURL[] =
    "https://translate.google.com/translate_a/element.js";
const char TranslateScript::kRequestHeader[] =
    "Google-Translate-Element-Mode: library";
const char TranslateScript::kAlwaysUseSslQueryName[] = "aus";
const char TranslateScript::kAlwaysUseSslQueryValue[] = "true";
const char TranslateScript::kCallbackQueryName[] = "cb";
const char TranslateScript::kCallbackQueryValue[] =
    "cr.googleTranslate.onTranslateElementLoad";
const char TranslateScript::kCssLoaderCallbackQueryName[] = "clc";
const char TranslateScript::kCssLoaderCallbackQueryValue[] =
    "cr.googleTranslate.onLoadCSS";
const char TranslateScript::kJavascriptLoaderCallbackQueryName[] = "jlc";
const char TranslateScript::kJavascriptLoaderCallbackQueryValue[] =
    "cr.googleTranslate.onLoadJavascript";

TranslateScript::TranslateScript()
    : weak_method_factory_(this),
      expiration_delay_(base::TimeDelta::FromDays(kExpirationDelayDays)) {
}

TranslateScript::~TranslateScript() {
  weak_method_factory_.InvalidateWeakPtrs();
}

void TranslateScript::Request(const Callback& callback) {
  if (fetcher_.get() != NULL) {
    NOTREACHED();
    return;
  }

  callback_ = callback;

  GURL translate_script_url;
  // Check if command-line contains an alternative URL for translate service.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTranslateScriptURL)) {
    translate_script_url = GURL(
        command_line.GetSwitchValueASCII(switches::kTranslateScriptURL));
    if (!translate_script_url.is_valid() ||
        !translate_script_url.query().empty()) {
      LOG(WARNING) << "The following translate URL specified at the "
                   << "command-line is invalid: "
                   << translate_script_url.spec();
      translate_script_url = GURL();
    }
  }

  // Use default URL when command-line argument is not specified, or specified
  // URL is invalid.
  if (translate_script_url.is_empty())
    translate_script_url = GURL(kScriptURL);

  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kCallbackQueryName,
      kCallbackQueryValue);
  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kAlwaysUseSslQueryName,
      kAlwaysUseSslQueryValue);
#if !defined(OS_IOS)
  // iOS doesn't need to use specific loaders for the isolated world.
  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kCssLoaderCallbackQueryName,
      kCssLoaderCallbackQueryValue);
  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kJavascriptLoaderCallbackQueryName,
      kJavascriptLoaderCallbackQueryValue);
#endif  // !defined(OS_IOS)

  translate_script_url =
      TranslateURLUtil::AddHostLocaleToUrl(translate_script_url);
  translate_script_url =
      TranslateURLUtil::AddApiKeyToUrl(translate_script_url);

  fetcher_.reset(new TranslateURLFetcher(kFetcherId));
  fetcher_->set_extra_request_header(kRequestHeader);
  fetcher_->Request(
      translate_script_url,
      base::Bind(&TranslateScript::OnScriptFetchComplete,
                 base::Unretained(this)));
}


void TranslateScript::OnScriptFetchComplete(
    int id, bool success, const std::string& data) {
  DCHECK_EQ(kFetcherId, id);

  scoped_ptr<const TranslateURLFetcher> delete_ptr(fetcher_.release());

  if (success) {
    DCHECK(data_.empty());
    // Insert variable definitions on API Key and security origin.
    data_ = base::StringPrintf("var translateApiKey = '%s';\n",
                               google_apis::GetAPIKey().c_str());

    GURL security_origin = translate::GetTranslateSecurityOrigin();
    base::StringAppendF(
        &data_, "var securityOrigin = '%s';", security_origin.spec().c_str());

    // Append embedded translate.js and a remote element library.
    base::StringPiece str = ResourceBundle::GetSharedInstance().
        GetRawDataResource(IDR_TRANSLATE_JS);
    str.AppendToString(&data_);
    data_ += data;

    // We'll expire the cached script after some time, to make sure long
    // running browsers still get fixes that might get pushed with newer
    // scripts.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TranslateScript::Clear,
                   weak_method_factory_.GetWeakPtr()),
        expiration_delay_);
  }

  callback_.Run(success, data);
}
