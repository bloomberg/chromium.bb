// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sms/sms_keyed_service.h"

#include "content/public/browser/sms_service.h"

SmsKeyedService::SmsKeyedService()
    : KeyedService(), service_(content::SmsService::Create()) {}

SmsKeyedService::~SmsKeyedService() = default;

content::SmsService* SmsKeyedService::Get() {
  return service_.get();
}
