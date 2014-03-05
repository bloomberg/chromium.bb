// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "policy/policy_constants.h"

// This policy loader loads a managed app configuration from the NSUserDefaults.
// For example code from Apple see:
// https://developer.apple.com/library/ios/samplecode/sc2279/Introduction/Intro.html
// For an introduction to the API see session 301 from WWDC 2013,
// "Extending Your Apps for Enterprise and Education Use":
// https://developer.apple.com/videos/wwdc/2013/?id=301

namespace {

// Key in the NSUserDefaults that contains the managed app configuration.
NSString* const kConfigurationKey = @"com.apple.configuration.managed";

// Key in the managed app configuration that contains the Chrome policy.
NSString* const kChromePolicyKey = @"ChromePolicy";

}  // namespace

// Helper that observes notifications for NSUserDefaults and triggers an update
// at the loader on the right thread.
@interface PolicyNotificationObserver : NSObject {
  base::Closure callback_;
  scoped_refptr<base::SequencedTaskRunner> taskRunner_;
}

// Designated initializer. |callback| will be posted to |taskRunner| whenever
// the NSUserDefaults change.
- (id)initWithCallback:(const base::Closure&)callback
            taskRunner:(scoped_refptr<base::SequencedTaskRunner>)taskRunner;

// Invoked when the NSUserDefaults change.
- (void)userDefaultsChanged:(NSNotification*)notification;

- (void)dealloc;

@end

@implementation PolicyNotificationObserver

- (id)initWithCallback:(const base::Closure&)callback
            taskRunner:(scoped_refptr<base::SequencedTaskRunner>)taskRunner {
  if ((self = [super init])) {
    callback_ = callback;
    taskRunner_ = taskRunner;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(userDefaultsChanged:)
               name:NSUserDefaultsDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)userDefaultsChanged:(NSNotification*)notification {
  // This may be invoked on any thread. Post the |callback_| to the loader's
  // |taskRunner_| to make sure it Reloads() on the right thread.
  taskRunner_->PostTask(FROM_HERE, callback_);
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

@end

namespace policy {

PolicyLoaderIOS::PolicyLoaderIOS(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner),
      weak_factory_(this) {}

PolicyLoaderIOS::~PolicyLoaderIOS() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());
}

void PolicyLoaderIOS::InitOnBackgroundThread() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());
  base::Closure callback = base::Bind(&PolicyLoaderIOS::UserDefaultsChanged,
                                      weak_factory_.GetWeakPtr());
  notification_observer_.reset(
      [[PolicyNotificationObserver alloc] initWithCallback:callback
                                                taskRunner:task_runner()]);
}

scoped_ptr<PolicyBundle> PolicyLoaderIOS::Load() {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  NSDictionary* configuration = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kConfigurationKey];
  id chromePolicy = configuration[kChromePolicyKey];
  if (chromePolicy && [chromePolicy isKindOfClass:[NSDictionary class]]) {
    // NSDictionary is toll-free bridged to CFDictionaryRef, which is a
    // CFPropertyListRef.
    scoped_ptr<base::Value> value =
        PropertyToValue(static_cast<CFPropertyListRef>(chromePolicy));
    base::DictionaryValue* dict = NULL;
    if (value && value->GetAsDictionary(&dict)) {
      PolicyMap& map = bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
      map.LoadFrom(dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
    }
  }

  return bundle.Pass();
}

base::Time PolicyLoaderIOS::LastModificationTime() {
  return last_notification_time_;
}

void PolicyLoaderIOS::UserDefaultsChanged() {
  // The base class coalesces multiple Reload() calls into a single Load() if
  // the LastModificationTime() has a small delta between Reload() calls.
  // This coalesces the multiple notifications sent during startup into a single
  // Load() call.
  last_notification_time_ = base::Time::Now();
  Reload(false);
}

}  // namespace policy
