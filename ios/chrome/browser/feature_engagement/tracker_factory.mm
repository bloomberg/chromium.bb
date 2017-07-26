// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/feature_engagement/tracker_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const base::FilePath::CharType kIOSFeatureEngagementTrackerStorageDirname[] =
    FILE_PATH_LITERAL("Feature Engagement Tracker");

}  // namespace

namespace feature_engagement {

// static
TrackerFactory* TrackerFactory::GetInstance() {
  return base::Singleton<TrackerFactory>::get();
}

// static
feature_engagement::Tracker* TrackerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<feature_engagement::Tracker*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

TrackerFactory::TrackerFactory()
    : BrowserStateKeyedServiceFactory(
          "feature_engagement::Tracker",
          BrowserStateDependencyManager::GetInstance()) {}

TrackerFactory::~TrackerFactory() = default;

std::unique_ptr<KeyedService> TrackerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND});

  base::FilePath storage_dir = browser_state->GetStatePath().Append(
      kIOSFeatureEngagementTrackerStorageDirname);

  return base::WrapUnique(
      feature_engagement::Tracker::Create(storage_dir, background_task_runner));
}

// Finds which browser state to use. If |context| is an incognito browser
// state, it returns the non-incognito state. Thus, feature engagement events
// are tracked even in incognito tabs.
web::BrowserState* TrackerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace feature_engagement
