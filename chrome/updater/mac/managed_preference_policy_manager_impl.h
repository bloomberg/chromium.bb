// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_IMPL_H_
#define CHROME_UPDATER_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_IMPL_H_

#import <Foundation/Foundation.h>

// TODO: crbug/1073980
//     Add a doc link for the managed preferences dictionary format.
//
// An example of the managed preferences policy dictionary in plist format:
//  <dict>
//    <key>updatePolicies</key>
//    <dict>
//      <key>global</key>
//      <dict>
//        <key>UpdateDefault</key>
//          <integer>3</integer>
//        <key>DownloadPreference</key>
//          <string>cacheable</string>
//        <key>UpdatesSuppressedStartHour</key>
//          <integer>16</integer>
//        <key>UpdatesSuppressedStartMin</key>
//          <integer>0</integer>
//        <key>UpdatesSuppressedDurationMin</key>
//          <integer>30</integer>
//      </dict>
//
//      <key>com.google.Chrome</key>
//      <dict>
//        <key>UpdateDefault</key>
//          <integer>2</integer>
//        <key>TargetVersionPrefix</key>
//          <string>82.</string>
//      </dict>
//    </dict>
//  </dict>

constexpr int kPolicyNotSet = -1;

using CRUAppPolicyDictionary = NSDictionary<NSString*, id>;
using CRUUpdatePolicyDictionary =
    NSDictionary<NSString*, CRUAppPolicyDictionary*>;

// Structure describes time window when update check is suppressed.
struct CRUUpdatesSuppressed {
  int start_hour = kPolicyNotSet;
  int start_minute = kPolicyNotSet;
  int duration_minute = kPolicyNotSet;
};

@interface CRUManagedPreferencePolicyManager : NSObject

@property(nonatomic, readonly, nullable) NSString* source;
@property(nonatomic, readonly) BOOL managed;

// Global-level policies.
@property(nonatomic, readonly) int lastCheckPeriodMinutes;
@property(nonatomic, readonly) int defaultUpdatePolicy;
@property(nonatomic, readonly, nullable) NSString* downloadPreference;
@property(nonatomic, readonly) CRUUpdatesSuppressed updatesSuppressed;
@property(nonatomic, readonly, nullable) NSString* proxyMode;
@property(nonatomic, readonly, nullable) NSString* proxyServer;
@property(nonatomic, readonly, nullable) NSString* proxyPacURL;

// App-level policies.
- (int)appUpdatePolicy:(nonnull NSString*)appid;
- (nullable NSString*)targetVersionPrefix:(nonnull NSString*)appid;
- (int)rollbackToTargetVersion:(nonnull NSString*)appid;

// |policies| should be the dictionary value read from managed preferences
// under the key "updatePolicies".
- (nullable instancetype)initWithDictionary:
    (nullable CRUUpdatePolicyDictionary*)policies;

@end

#endif  // CHROME_UPDATER_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_IMPL_H_
