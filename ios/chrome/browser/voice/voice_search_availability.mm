// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_availability.h"

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

VoiceSearchAvailability::VoiceSearchAvailability()
    : observers_(static_cast<id>([CRBProtocolObservers
          observersWithProtocol:@protocol(VoiceSearchAvailabilityObserver)])) {
  voice_over_enabled_ = IsVoiceOverEnabled();
  notification_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIAccessibilityVoiceOverStatusDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull note) {
                // This block will be unregistered in VoiceSearchAvailability's
                // destructor, thus the captured "this" won't outlive the
                // current instance which makes this code safe.
                SetIsVoiceOverEnabled(IsVoiceOverEnabled());
              }];
}

VoiceSearchAvailability::~VoiceSearchAvailability() {
  [NSNotificationCenter.defaultCenter removeObserver:notification_observer_];
}

void VoiceSearchAvailability::AddObserver(
    id<VoiceSearchAvailabilityObserver> observer) {
  [observers_ addObserver:observer];
}

void VoiceSearchAvailability::RemoveObserver(
    id<VoiceSearchAvailabilityObserver> observer) {
  [observers_ removeObserver:observer];
}

bool VoiceSearchAvailability::IsVoiceSearchAvailable() const {
  return !voice_over_enabled_ && ios::GetChromeBrowserProvider()
                                     ->GetVoiceSearchProvider()
                                     ->IsVoiceSearchEnabled();
}

bool VoiceSearchAvailability::IsVoiceOverEnabled() const {
  return UIAccessibilityIsVoiceOverRunning();
}

void VoiceSearchAvailability::SetIsVoiceOverEnabled(bool enabled) {
  if (voice_over_enabled_ == enabled)
    return;

  bool previously_available = IsVoiceSearchAvailable();
  voice_over_enabled_ = enabled;
  bool available = IsVoiceSearchAvailable();
  if (available == previously_available)
    return;

  [observers_ voiceSearchAvailability:this updatedAvailability:available];
}
