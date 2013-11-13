// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/android/prerender_condition_platform.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

namespace android {

namespace {

const char kAllowPrerender[] = "allow-prerender";

class BooleanWrapper : public base::SupportsUserData::Data {
 public:
  explicit BooleanWrapper(bool b) : m_b(b) { }
  virtual ~BooleanWrapper() { }

  operator bool() const { return m_b; }
 private:
  bool m_b;
  DISALLOW_COPY_AND_ASSIGN(BooleanWrapper);
};

}  // namespace

PrerenderConditionPlatform::PrerenderConditionPlatform(
    content::BrowserContext* context)
    : context_(context) {}

PrerenderConditionPlatform::~PrerenderConditionPlatform() {}

bool PrerenderConditionPlatform::CanPrerender() const {
  base::SupportsUserData::Data* data = context_->GetUserData(kAllowPrerender);
  if (!data)
    return true;
  BooleanWrapper* b = static_cast<BooleanWrapper*>(data);
  return *b;
}

void PrerenderConditionPlatform::SetEnabled(content::BrowserContext* context,
                                            bool enabled) {
  BooleanWrapper* wrapper = new BooleanWrapper(enabled);
  context->SetUserData(kAllowPrerender, wrapper);
}

}  // namespace android
