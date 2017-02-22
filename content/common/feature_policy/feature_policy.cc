// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/feature_policy/feature_policy.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace content {

namespace {

// Given a string name, return the matching feature struct, or nullptr if it is
// not the name of a policy-controlled feature.
blink::WebFeaturePolicyFeature FeatureForName(
    const std::string& feature_name,
    const FeaturePolicy::FeatureList& features) {
  for (const auto& feature_mapping : features) {
    if (feature_name == feature_mapping.second->feature_name)
      return feature_mapping.first;
  }
  return blink::WebFeaturePolicyFeature::NotFound;
}

// Definitions of all features controlled by Feature Policy should appear here.
const FeaturePolicy::Feature kDocumentCookie{
    "cookie", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kDocumentDomain{
    "domain", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kDocumentWrite{
    "docwrite", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kFullscreenFeature{
    "fullscreen", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kGeolocationFeature{
    "geolocation", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kMidiFeature{
    "midi", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kNotificationsFeature{
    "notifications", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kPaymentFeature{
    "payment", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kPushFeature{
    "push", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kSyncScript{
    "sync-script", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kSyncXHR{
    "sync-xhr", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kUsermedia{
    "usermedia", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kVibrateFeature{
    "vibrate", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kWebRTC{
    "webrtc", FeaturePolicy::FeatureDefault::EnableForAll};

// Extracts a Whitelist from a ParsedFeaturePolicyDeclaration.
std::unique_ptr<FeaturePolicy::Whitelist> WhitelistFromDeclaration(
    const ParsedFeaturePolicyDeclaration& parsed_declaration) {
  std::unique_ptr<FeaturePolicy::Whitelist> result =
      base::WrapUnique(new FeaturePolicy::Whitelist());
  if (parsed_declaration.matches_all_origins)
    result->AddAll();
  for (const auto& origin : parsed_declaration.origins)
    result->Add(origin);
  return result;
}

}  // namespace

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration()
    : matches_all_origins(false) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    std::string feature_name,
    bool matches_all_origins,
    std::vector<url::Origin> origins)
    : feature_name(feature_name),
      matches_all_origins(matches_all_origins),
      origins(origins) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration::~ParsedFeaturePolicyDeclaration() {}

FeaturePolicy::Whitelist::Whitelist() : matches_all_origins_(false) {}

FeaturePolicy::Whitelist::~Whitelist() = default;

void FeaturePolicy::Whitelist::Add(const url::Origin& origin) {
  origins_.push_back(origin);
}

void FeaturePolicy::Whitelist::AddAll() {
  matches_all_origins_ = true;
}

bool FeaturePolicy::Whitelist::Contains(const url::Origin& origin) const {
  if (matches_all_origins_)
    return true;
  for (const auto& targetOrigin : origins_) {
    if (targetOrigin.IsSameOriginWith(origin))
      return true;
  }
  return false;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicyHeader* container_policy,
    const url::Origin& origin) {
  return CreateFromParentPolicy(parent_policy, container_policy, origin,
                                GetDefaultFeatureList());
}

bool FeaturePolicy::IsFeatureEnabledForOrigin(
    blink::WebFeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::ContainsKey(feature_list_, feature));
  const FeaturePolicy::Feature* feature_definition = feature_list_.at(feature);
  DCHECK(base::ContainsKey(inherited_policies_, feature));
  if (!inherited_policies_.at(feature))
    return false;
  auto whitelist = whitelists_.find(feature);
  if (whitelist != whitelists_.end())
    return whitelist->second->Contains(origin);
  if (feature_definition->default_policy ==
      FeaturePolicy::FeatureDefault::EnableForAll) {
    return true;
  }
  if (feature_definition->default_policy ==
      FeaturePolicy::FeatureDefault::EnableForSelf) {
    return origin_.IsSameOriginWith(origin);
  }
  return false;
}

bool FeaturePolicy::IsFeatureEnabled(
    blink::WebFeaturePolicyFeature feature) const {
  return IsFeatureEnabledForOrigin(feature, origin_);
}

void FeaturePolicy::SetHeaderPolicy(
    const ParsedFeaturePolicyHeader& parsed_header) {
  DCHECK(whitelists_.empty());
  for (const ParsedFeaturePolicyDeclaration& parsed_declaration :
       parsed_header) {
    blink::WebFeaturePolicyFeature feature =
        FeatureForName(parsed_declaration.feature_name, feature_list_);
    if (feature == blink::WebFeaturePolicyFeature::NotFound)
      continue;
    whitelists_[feature] = WhitelistFromDeclaration(parsed_declaration);
  }
}

FeaturePolicy::FeaturePolicy(url::Origin origin,
                             const FeatureList& feature_list)
    : origin_(origin), feature_list_(feature_list) {}

FeaturePolicy::FeaturePolicy(url::Origin origin)
    : origin_(origin), feature_list_(GetDefaultFeatureList()) {}

FeaturePolicy::~FeaturePolicy() {}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicyHeader* container_policy,
    const url::Origin& origin,
    const FeaturePolicy::FeatureList& features) {
  std::unique_ptr<FeaturePolicy> new_policy =
      base::WrapUnique(new FeaturePolicy(origin, features));
  for (const auto& feature : features) {
    if (!parent_policy ||
        parent_policy->IsFeatureEnabledForOrigin(feature.first, origin)) {
      new_policy->inherited_policies_[feature.first] = true;
    } else {
      new_policy->inherited_policies_[feature.first] = false;
    }
    if (container_policy)
      new_policy->AddContainerPolicy(container_policy, parent_policy);
  }
  return new_policy;
}

void FeaturePolicy::AddContainerPolicy(
    const ParsedFeaturePolicyHeader* container_policy,
    const FeaturePolicy* parent_policy) {
  DCHECK(container_policy);
  DCHECK(parent_policy);
  for (const ParsedFeaturePolicyDeclaration& parsed_declaration :
       *container_policy) {
    // If a feature is enabled in the parent frame, and the parent chooses to
    // delegate it to the child frame, using the iframe attribute, then the
    // feature should be enabled in the child frame.
    blink::WebFeaturePolicyFeature feature =
        FeatureForName(parsed_declaration.feature_name, feature_list_);
    if (feature == blink::WebFeaturePolicyFeature::NotFound)
      continue;
    if (WhitelistFromDeclaration(parsed_declaration)->Contains(origin_) &&
        parent_policy->IsFeatureEnabled(feature)) {
      inherited_policies_[feature] = true;
    } else {
      inherited_policies_[feature] = false;
    }
  }
}

// static
const FeaturePolicy::FeatureList& FeaturePolicy::GetDefaultFeatureList() {
  CR_DEFINE_STATIC_LOCAL(
      FeatureList, default_feature_list,
      ({{blink::WebFeaturePolicyFeature::DocumentCookie, &kDocumentCookie},
        {blink::WebFeaturePolicyFeature::DocumentDomain, &kDocumentDomain},
        {blink::WebFeaturePolicyFeature::DocumentWrite, &kDocumentWrite},
        {blink::WebFeaturePolicyFeature::Fullscreen, &kFullscreenFeature},
        {blink::WebFeaturePolicyFeature::Geolocation, &kGeolocationFeature},
        {blink::WebFeaturePolicyFeature::MidiFeature, &kMidiFeature},
        {blink::WebFeaturePolicyFeature::Notifications, &kNotificationsFeature},
        {blink::WebFeaturePolicyFeature::Payment, &kPaymentFeature},
        {blink::WebFeaturePolicyFeature::Push, &kPushFeature},
        {blink::WebFeaturePolicyFeature::SyncScript, &kSyncScript},
        {blink::WebFeaturePolicyFeature::SyncXHR, &kSyncXHR},
        {blink::WebFeaturePolicyFeature::Usermedia, &kUsermedia},
        {blink::WebFeaturePolicyFeature::Vibrate, &kVibrateFeature},
        {blink::WebFeaturePolicyFeature::WebRTC, &kWebRTC}}));
  return default_feature_list;
}

}  // namespace content
