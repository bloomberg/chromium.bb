// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include "chrome/browser/profiles/profile.h"


HotwordService::HotwordService(Profile* profile)
    : profile_(profile) {
}

HotwordService::~HotwordService() {
}
