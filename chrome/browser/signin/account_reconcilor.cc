// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor.h"

#include "chrome/browser/profiles/profile.h"

AccountReconcilor::AccountReconcilor(Profile* profile) : profile_(profile) {
}

AccountReconcilor::~AccountReconcilor() {}

void AccountReconcilor::Shutdown() {}

