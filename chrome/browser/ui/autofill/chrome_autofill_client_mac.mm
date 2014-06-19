// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "components/autofill/core/browser/personal_data_manager.h"

// Listens to Keystone notifications and passes relevant ones on to the
// PersonalDataManager.
@interface AutofillKeystoneBridge : NSObject {
 @private
  // The PersonalDataManager, if it exists, is expected to outlive this object.
  autofill::PersonalDataManager* personal_data_manager_;
}

// Designated initializer. The PersonalDataManager, if it exists, is expected
// to outlive this object. The PersonalDataManager may be NULL in tests.
- (instancetype)initWithPersonalDataManager:
    (autofill::PersonalDataManager*)personal_data_manager;

// Receieved a notification from Keystone.
- (void)handleStatusNotification:(NSNotification*)notification;
@end

@implementation AutofillKeystoneBridge

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithPersonalDataManager:
    (autofill::PersonalDataManager*)personal_data_manager {
  self = [super init];
  if (self) {
    personal_data_manager_ = personal_data_manager;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(handleStatusNotification:)
                   name:kAutoupdateStatusNotification
                 object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)handleStatusNotification:(NSNotification*)notification {
  if (!personal_data_manager_)
    return;

  NSNumber* statusNumber =
      [[notification userInfo] objectForKey:kAutoupdateStatusStatus];
  DCHECK(statusNumber);
  DCHECK([statusNumber isKindOfClass:[NSNumber class]]);
  AutoupdateStatus status =
      static_cast<AutoupdateStatus>([statusNumber intValue]);

  switch (status) {
    case kAutoupdateInstalling:
    case kAutoupdateInstalled: {
      personal_data_manager_->BinaryChanging();
      return;
    }
    default:
      return;
  }
}

@end

namespace autofill {

class AutofillKeystoneBridgeWrapper {
 public:
  explicit AutofillKeystoneBridgeWrapper(
      PersonalDataManager* personal_data_manager) {
    bridge_.reset([[AutofillKeystoneBridge alloc]
        initWithPersonalDataManager:personal_data_manager]);
  }
  ~AutofillKeystoneBridgeWrapper() {}

 private:
  base::scoped_nsobject<AutofillKeystoneBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillKeystoneBridgeWrapper);
};

void ChromeAutofillClient::RegisterForKeystoneNotifications() {
  bridge_wrapper_ = new AutofillKeystoneBridgeWrapper(GetPersonalDataManager());
}

void ChromeAutofillClient::UnregisterFromKeystoneNotifications() {
  delete bridge_wrapper_;
}

}  // namespace autofill
