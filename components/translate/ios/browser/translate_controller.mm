// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/js_translate_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/web_state.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

namespace {
// Prefix for the translate javascript commands. Must be kept in sync with
// translate_ios.js.
const char kCommandPrefix[] = "translate";
}

TranslateController::TranslateController(web::WebState* web_state,
                                         JsTranslateManager* manager)
    : web_state_(web_state),
      observer_(nullptr),
      js_manager_(manager),
      weak_method_factory_(this) {
  DCHECK(js_manager_);
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  web_state_->AddScriptCommandCallback(
      base::Bind(&TranslateController::OnJavascriptCommandReceived,
                 base::Unretained(this)),
      kCommandPrefix);
}

TranslateController::~TranslateController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void TranslateController::InjectTranslateScript(
    const std::string& translate_script) {
  [js_manager_ setScript:base::SysUTF8ToNSString(translate_script)];
  [js_manager_ inject];
}

void TranslateController::RevertTranslation() {
  [js_manager_ revertTranslation];
}

void TranslateController::StartTranslation(const std::string& source_language,
                                           const std::string& target_language) {
  [js_manager_ startTranslationFrom:source_language to:target_language];
}

void TranslateController::SetJsTranslateManagerForTesting(
    JsTranslateManager* manager) {
  js_manager_.reset(manager);
}

bool TranslateController::OnJavascriptCommandReceived(
    const base::DictionaryValue& command,
    const GURL& url,
    bool interacting,
    bool is_main_frame) {
  if (!is_main_frame) {
    // Translate is only supported on main frame.
    return false;
  }
  const base::Value* value = nullptr;
  command.Get("command", &value);
  if (!value) {
    return false;
  }

  std::string out_string;
  value->GetAsString(&out_string);
  if (out_string == "translate.ready")
    return OnTranslateReady(command);
  if (out_string == "translate.status")
    return OnTranslateComplete(command);
  if (out_string == "translate.loadjavascript")
    return OnTranslateLoadJavaScript(command);

  return false;
}

bool TranslateController::OnTranslateReady(
    const base::DictionaryValue& command) {
  double error_code = 0.;
  double load_time = 0.;
  double ready_time = 0.;

  if (!command.HasKey("errorCode") ||
      !command.GetDouble("errorCode", &error_code) ||
      error_code < TranslateErrors::NONE ||
      error_code >= TranslateErrors::TRANSLATE_ERROR_MAX) {
    return false;
  }

  TranslateErrors::Type error_type =
      static_cast<TranslateErrors::Type>(error_code);
  if (error_type == TranslateErrors::NONE) {
    if (!command.HasKey("loadTime") || !command.HasKey("readyTime")) {
      return false;
    }
    command.GetDouble("loadTime", &load_time);
    command.GetDouble("readyTime", &ready_time);
  }
  if (observer_)
    observer_->OnTranslateScriptReady(error_type, load_time, ready_time);
  return true;
}

bool TranslateController::OnTranslateComplete(
    const base::DictionaryValue& command) {
  double error_code = 0.;
  std::string original_language;
  double translation_time = 0.;

  if (!command.HasKey("errorCode") ||
      !command.GetDouble("errorCode", &error_code) ||
      error_code < TranslateErrors::NONE ||
      error_code >= TranslateErrors::TRANSLATE_ERROR_MAX) {
    return false;
  }

  TranslateErrors::Type error_type =
      static_cast<TranslateErrors::Type>(error_code);
  if (error_type == TranslateErrors::NONE) {
    if (!command.HasKey("originalPageLanguage") ||
        !command.HasKey("translationTime")) {
      return false;
    }
    command.GetString("originalPageLanguage", &original_language);
    command.GetDouble("translationTime", &translation_time);
  }

  if (observer_)
    observer_->OnTranslateComplete(error_type, original_language,
                                   translation_time);
  return true;
}

bool TranslateController::OnTranslateLoadJavaScript(
    const base::DictionaryValue& command) {
  std::string url;
  if (!command.HasKey("url") || !command.GetString("url", &url)) {
    return false;
  }

  FetchScript(url);

  return true;
}

void TranslateController::FetchScript(const std::string& url) {
  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url.find(security_origin.spec()) || script_fetcher_) {
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);

  script_fetcher_ = network::SimpleURLLoader::Create(
      std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);
  script_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      web_state_->GetBrowserState()->GetURLLoaderFactory(),
      base::BindOnce(&TranslateController::OnScriptFetchComplete,
                     base::Unretained(this)));
}

void TranslateController::OnScriptFetchComplete(
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    web_state_->ExecuteJavaScript(base::UTF8ToUTF16(*response_body));
  }
  script_fetcher_.reset();
}

// web::WebStateObserver implementation.

void TranslateController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveScriptCommandCallback(kCommandPrefix);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;

  script_fetcher_.reset();
}

void TranslateController::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument()) {
    script_fetcher_.reset();
  }
}

}  // namespace translate
