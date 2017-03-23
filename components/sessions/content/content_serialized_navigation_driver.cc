// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_driver.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/common/page_state.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"

namespace sessions {

namespace {

const int kObsoleteReferrerPolicyAlways = 0;
const int kObsoleteReferrerPolicyDefault = 1;
const int kObsoleteReferrerPolicyNever = 2;
const int kObsoleteReferrerPolicyOrigin = 3;

ContentSerializedNavigationDriver* g_instance = nullptr;

}  // namespace

// static
SerializedNavigationDriver* SerializedNavigationDriver::Get() {
  return ContentSerializedNavigationDriver::GetInstance();
}

// static
ContentSerializedNavigationDriver*
ContentSerializedNavigationDriver::GetInstance() {
  if (g_instance)
    return g_instance;

  auto* instance = base::Singleton<
      ContentSerializedNavigationDriver,
      base::LeakySingletonTraits<ContentSerializedNavigationDriver>>::get();
  g_instance = instance;
  return instance;
}

// static
void ContentSerializedNavigationDriver::SetInstance(
    ContentSerializedNavigationDriver* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

ContentSerializedNavigationDriver::ContentSerializedNavigationDriver() {
}

ContentSerializedNavigationDriver::~ContentSerializedNavigationDriver() {
}

int ContentSerializedNavigationDriver::GetDefaultReferrerPolicy() const {
  return blink::WebReferrerPolicyDefault;
}

bool ContentSerializedNavigationDriver::MapReferrerPolicyToOldValues(
    int referrer_policy,
    int* mapped_referrer_policy) const {
  switch (referrer_policy) {
    case blink::WebReferrerPolicyAlways:
    case blink::WebReferrerPolicyDefault:
      // "always" and "default" are the same value in all versions.
      *mapped_referrer_policy = referrer_policy;
      return true;

    case blink::WebReferrerPolicyOrigin:
      // "origin" exists in the old encoding.
      *mapped_referrer_policy = kObsoleteReferrerPolicyOrigin;
      return true;

    default:
      // Everything else is mapped to never.
      *mapped_referrer_policy = kObsoleteReferrerPolicyNever;
      return false;
  }
}

bool ContentSerializedNavigationDriver::MapReferrerPolicyToNewValues(
    int referrer_policy,
    int* mapped_referrer_policy) const {
  switch (referrer_policy) {
    case kObsoleteReferrerPolicyAlways:
    case kObsoleteReferrerPolicyDefault:
      // "always" and "default" are the same value in all versions.
      *mapped_referrer_policy = referrer_policy;
      return true;

    default:
      // Since we don't know what encoding was used, we map the rest to "never".
      *mapped_referrer_policy = blink::WebReferrerPolicyNever;
      return false;
  }
}

std::string
ContentSerializedNavigationDriver::GetSanitizedPageStateForPickle(
    const SerializedNavigationEntry* navigation) const {
  if (!navigation->has_post_data())
    return navigation->encoded_page_state();

  content::PageState page_state = content::PageState::CreateFromEncodedData(
      navigation->encoded_page_state());
  return page_state.RemovePasswordData().ToEncodedData();
}

void ContentSerializedNavigationDriver::Sanitize(
    SerializedNavigationEntry* navigation) const {
}

std::string ContentSerializedNavigationDriver::StripReferrerFromPageState(
      const std::string& page_state) const {
  return content::PageState::CreateFromEncodedData(page_state)
      .RemoveReferrer()
      .ToEncodedData();
}

void ContentSerializedNavigationDriver::RegisterExtendedInfoHandler(
    const std::string& key,
    std::unique_ptr<ExtendedInfoHandler> handler) {
  DCHECK(!key.empty());
  DCHECK(!extended_info_handler_map_.count(key));
  DCHECK(handler.get());
  extended_info_handler_map_[key] = std::move(handler);
}

const ContentSerializedNavigationDriver::ExtendedInfoHandlerMap&
ContentSerializedNavigationDriver::GetAllExtendedInfoHandlers() const {
  return extended_info_handler_map_;
}

}  // namespace sessions
