// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reader_mode/reader_mode_checker.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/page_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/grit/components_resources.h"
#include "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#include "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ReaderModeCheckerObserver::ReaderModeCheckerObserver(
    ReaderModeChecker* readerModeChecker)
    : readerModeChecker_(readerModeChecker) {
  DCHECK(readerModeChecker);
  readerModeChecker_->AddObserver(this);
}

ReaderModeCheckerObserver::~ReaderModeCheckerObserver() {
  readerModeChecker_->RemoveObserver(this);
}

enum class ReaderModeChecker::Status {
  kSwitchNo,        // There is no way a switch could happen.
  kSwitchMaybe,     // Not sure, it may be possible.
  kSwitchPossible,  // It is possible to switch to reader mode.
};

ReaderModeChecker::ReaderModeChecker(web::WebState* web_state)
    : web::WebStateObserver(web_state),
      is_distillable_(Status::kSwitchNo),
      weak_factory_(this) {
  DCHECK(web_state);
}

ReaderModeChecker::~ReaderModeChecker() {
  for (auto& observer : observers_)
    observer.ReaderModeCheckerDestroyed();
}

bool ReaderModeChecker::CanSwitchToReaderMode() {
  return is_distillable_ != ReaderModeChecker::Status::kSwitchNo;
}

void ReaderModeChecker::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status != web::PageLoadCompletionStatus::SUCCESS)
    return;

  if (is_distillable_ == Status::kSwitchPossible)
    return;

  CheckIsDistillable();
}

void ReaderModeChecker::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_details) {
  // When NavigationItemCommitted the page may not be completely loaded. But in
  // order to get reader mode data faster this class tries to inject the
  // javascript early. If it fails, it will be retried at PageLoaded().
  is_distillable_ = Status::kSwitchNo;
  CheckIsDistillable();
}

void ReaderModeChecker::CheckIsDistillableOG(CRWJSInjectionReceiver* receiver) {
  // Retrieve the javascript used to figure out if the page is at all
  // distillable.
  NSString* is_distillable_js =
      base::SysUTF8ToNSString(dom_distiller::url_utils::GetIsDistillableJs());
  DCHECK(is_distillable_js);

  // In case |this| gets deallocated before the block executes, the block only
  // references a weak pointer.
  base::WeakPtr<ReaderModeChecker> weak_this(weak_factory_.GetWeakPtr());
  [receiver executeJavaScript:is_distillable_js
            completionHandler:^(id result, NSError* error) {
              if (!weak_this || error || !result)
                return;  // Inconclusive.
              if ([result isEqual:@YES]) {
                weak_this->is_distillable_ = Status::kSwitchPossible;
                for (auto& observer : weak_this->observers_)
                  observer.PageIsDistillable();
              } else {
                weak_this->is_distillable_ = Status::kSwitchMaybe;
              }
            }];
}

void ReaderModeChecker::CheckIsDistillableDetector(
    CRWJSInjectionReceiver* receiver) {
  // Retrieve the javascript used to figure out if the page is at all
  // distillable.
  NSString* extract_features_js = base::SysUTF8ToNSString(
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(IDR_EXTRACT_PAGE_FEATURES_JS)
          .as_string());
  DCHECK(extract_features_js);

  // In case |this| gets deallocated before the block executes, the block only
  // references a weak pointer.
  base::WeakPtr<ReaderModeChecker> weak_this(weak_factory_.GetWeakPtr());
  [receiver
      executeJavaScript:extract_features_js
      completionHandler:^(id result, NSError* error) {
        if (!weak_this || error || ![result isKindOfClass:[NSString class]]) {
          return;  // Inconclusive.
        }
        const dom_distiller::DistillablePageDetector* detector =
            dom_distiller::DistillablePageDetector::GetDefault();
        const base::StringValue value(base::SysNSStringToUTF8(result));
        std::vector<double> features(
            dom_distiller::CalculateDerivedFeaturesFromJSON(&value));
        if (detector->Classify(features)) {
          weak_this->is_distillable_ = Status::kSwitchPossible;
          for (auto& observer : weak_this->observers_)
            observer.PageIsDistillable();
        } else {
          weak_this->is_distillable_ = Status::kSwitchMaybe;
        }
      }];
}

void ReaderModeChecker::CheckIsDistillable() {
  CRWJSInjectionReceiver* receiver = web_state()->GetJSInjectionReceiver();

  if (!receiver || !web_state()->ContentIsHTML() ||
      web_state()->IsShowingWebInterstitial()) {
    is_distillable_ = Status::kSwitchNo;
    return;
  }

  switch (dom_distiller::GetDistillerHeuristicsType()) {
    case dom_distiller::DistillerHeuristicsType::NONE:
      is_distillable_ = Status::kSwitchMaybe;
      break;
    case dom_distiller::DistillerHeuristicsType::OG_ARTICLE:
      CheckIsDistillableOG(receiver);
      break;
    case dom_distiller::DistillerHeuristicsType::ADABOOST_MODEL:
      CheckIsDistillableDetector(receiver);
      break;
    case dom_distiller::DistillerHeuristicsType::ALWAYS_TRUE:
      is_distillable_ = Status::kSwitchPossible;
      break;
  }
}

void ReaderModeChecker::WebStateDestroyed() {
  is_distillable_ = Status::kSwitchNo;
}

void ReaderModeChecker::AddObserver(ReaderModeCheckerObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void ReaderModeChecker::RemoveObserver(ReaderModeCheckerObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}
