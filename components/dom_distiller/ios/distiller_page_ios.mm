// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_ios.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// This is duplicated here from ios/web/web_state/ui/web_view_js_utils.mm in
// order to handle numbers. The dom distiller proto expects integers and the
// generated JSON deserializer does not accept doubles in the place of ints.
// However WKWebView only returns "numbers." However, here the proto expects
// integers and doubles, which is done by checking if the number has a fraction
// or not; since this is a hacky method it's isolated to this file so as to
// limit the risk of broken JS calls.

int const kMaximumParsingRecursionDepth = 6;
// Converts result of WKWebView script evaluation to base::Value, parsing
// |wk_result| up to a depth of |max_depth|.
std::unique_ptr<base::Value> ValueResultFromScriptResult(id wk_result,
                                                         int max_depth) {
  if (!wk_result)
    return nullptr;

  std::unique_ptr<base::Value> result;

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  CFTypeID result_type = CFGetTypeID(reinterpret_cast<CFTypeRef>(wk_result));
  if (result_type == CFStringGetTypeID()) {
    result.reset(new base::Value(base::SysNSStringToUTF16(wk_result)));
    DCHECK(result->IsType(base::Value::Type::STRING));
  } else if (result_type == CFNumberGetTypeID()) {
    // Different implementation is here.
    if ([wk_result intValue] != [wk_result doubleValue]) {
      result.reset(new base::Value([wk_result doubleValue]));
      DCHECK(result->IsType(base::Value::Type::DOUBLE));
    } else {
      result.reset(new base::Value([wk_result intValue]));
      DCHECK(result->IsType(base::Value::Type::INTEGER));
    }
    // End of different implementation.
  } else if (result_type == CFBooleanGetTypeID()) {
    result.reset(new base::Value(static_cast<bool>([wk_result boolValue])));
    DCHECK(result->IsType(base::Value::Type::BOOLEAN));
  } else if (result_type == CFNullGetTypeID()) {
    result = base::MakeUnique<base::Value>();
    DCHECK(result->IsType(base::Value::Type::NONE));
  } else if (result_type == CFDictionaryGetTypeID()) {
    std::unique_ptr<base::DictionaryValue> dictionary =
        base::MakeUnique<base::DictionaryValue>();
    for (id key in wk_result) {
      NSString* obj_c_string = base::mac::ObjCCast<NSString>(key);
      const std::string path = base::SysNSStringToUTF8(obj_c_string);
      std::unique_ptr<base::Value> value =
          ValueResultFromScriptResult(wk_result[obj_c_string], max_depth - 1);
      if (value) {
        dictionary->Set(path, std::move(value));
      }
    }
    result = std::move(dictionary);
  } else if (result_type == CFArrayGetTypeID()) {
    std::unique_ptr<base::ListValue> list = base::MakeUnique<base::ListValue>();
    for (id list_item in wk_result) {
      std::unique_ptr<base::Value> value =
      ValueResultFromScriptResult(list_item, max_depth - 1);
      if (value) {
        list->Append(std::move(value));
      }
    }
    result = std::move(list);
  } else {
    NOTREACHED();  // Convert other types as needed.
  }
  return result;
}
}

namespace dom_distiller {

// Helper class for observing the loading of URLs to distill.
class DistillerWebStateObserver : public web::WebStateObserver {
 public:
  DistillerWebStateObserver(web::WebState* web_state,
                            DistillerPageIOS* distiller_page);

  // WebStateObserver implementation:
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed() override;
  void DidStartLoading() override;
  void DidStopLoading() override;

 private:
  DistillerPageIOS* distiller_page_;  // weak, owns this object.
  bool loading_;
};

DistillerWebStateObserver::DistillerWebStateObserver(
    web::WebState* web_state,
    DistillerPageIOS* distiller_page)
    : web::WebStateObserver(web_state),
      distiller_page_(distiller_page),
      loading_(false) {
  DCHECK(web_state);
  DCHECK(distiller_page_);
}

void DistillerWebStateObserver::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  if (!loading_) {
    return;
  }
  loading_ = false;
  distiller_page_->OnLoadURLDone(load_completion_status);
}

void DistillerWebStateObserver::WebStateDestroyed() {
  distiller_page_->DetachWebState();
}

void DistillerWebStateObserver::DidStartLoading() {
  loading_ = true;
}

void DistillerWebStateObserver::DidStopLoading() {
  if (web_state()->IsShowingWebInterstitial()) {
    // If there is an interstitial, stop the distillation.
    // The interstitial is not displayed to the user who cannot choose to
    // continue.
    PageLoaded(web::PageLoadCompletionStatus::FAILURE);
  }
}

#pragma mark -

DistillerPageIOS::DistillerPageIOS(web::BrowserState* browser_state)
    : browser_state_(browser_state), weak_ptr_factory_(this) {}

bool DistillerPageIOS::StringifyOutput() {
  return false;
}

DistillerPageIOS::~DistillerPageIOS() {}

void DistillerPageIOS::AttachWebState(
    std::unique_ptr<web::WebState> web_state) {
  if (web_state_) {
    DetachWebState();
  }
  web_state_ = std::move(web_state);
  if (web_state_) {
    web_state_observer_ =
        base::MakeUnique<DistillerWebStateObserver>(web_state_.get(), this);
  }
}

std::unique_ptr<web::WebState> DistillerPageIOS::DetachWebState() {
  std::unique_ptr<web::WebState> old_web_state = std::move(web_state_);
  web_state_observer_.reset();
  web_state_.reset();
  return old_web_state;
}

web::WebState* DistillerPageIOS::CurrentWebState() {
  return web_state_.get();
}

void DistillerPageIOS::DistillPageImpl(const GURL& url,
                                       const std::string& script) {
  if (!url.is_valid() || !script.length())
    return;
  url_ = url;
  script_ = script;

  if (!web_state_) {
    const web::WebState::CreateParams web_state_create_params(browser_state_);
    std::unique_ptr<web::WebState> web_state_unique =
        web::WebState::Create(web_state_create_params);
    AttachWebState(std::move(web_state_unique));
  }
  // Load page using WebState.
  web::NavigationManager::WebLoadParams params(url_);
  web_state_->SetWebUsageEnabled(true);
  web_state_->GetNavigationManager()->LoadURLWithParams(params);
  // GetView is needed because the view is not created (but needed) when
  // loading the page.
  web_state_->GetView();
}

void DistillerPageIOS::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  // Don't attempt to distill if the page load failed or if there is no
  // WebState.
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE ||
      !web_state_) {
    HandleJavaScriptResult(nil);
    return;
  }
  // Inject the script.
  base::WeakPtr<DistillerPageIOS> weak_this = weak_ptr_factory_.GetWeakPtr();
  [[web_state_->GetJSInjectionReceiver()
      instanceOfClass:[CRWJSInjectionManager class]]
      executeJavaScript:base::SysUTF8ToNSString(script_)
      completionHandler:^(id result, NSError* error) {
        DistillerPageIOS* distiller_page = weak_this.get();
        if (distiller_page)
          distiller_page->HandleJavaScriptResult(result);
      }];
}

void DistillerPageIOS::HandleJavaScriptResult(id result) {
  auto resultValue = base::MakeUnique<base::Value>();
  if (result) {
    resultValue = ValueResultFromScriptResult(result);
  }
  OnDistillationDone(url_, resultValue.get());
}

std::unique_ptr<base::Value> DistillerPageIOS::ValueResultFromScriptResult(
    id wk_result) {
  return ::ValueResultFromScriptResult(wk_result,
                                       kMaximumParsingRecursionDepth);
}
}  // namespace dom_distiller
