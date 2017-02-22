// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_
#define CONTENT_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"
#include "url/origin.h"

namespace content {

// Feature Policy is a mechanism for controlling the availability of web
// platform features in a frame, including all embedded frames. It can be used
// to remove features, automatically refuse API permission requests, or modify
// the behaviour of features. (The specific changes which are made depend on the
// feature; see the specification for details).
//
// Policies can be defined in the HTTP header stream, with the |Feature-Policy|
// HTTP header, or can be set by the |allow| attributes on the iframe element
// which embeds the document.
//
// See https://wicg.github.io/FeaturePolicy/
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined by instances of the
// FeaturePolicy::Feature struct. The features are referenced by the
// |WebFeaturePolicyFeature| enum, declared in |WebFeaturePolicy.h|.
//
// Whitelists
// ----------
// Whitelists are collections of origins, although two special terms can be used
// when declaring them:
//   "self" refers to the orgin of the frame which is declaring the policy.
//   "*" refers to all origins; any origin will match a whitelist which contains
//     it.
//
// Declarations
// ------------
// A feature policy declaration is a mapping of a feature name to a whitelist.
// A set of declarations is a declared policy.
//
// Inherited Policy
// ----------------
// In addition to the declared policy (which may be empty), every frame has
// an inherited policy, which is determined by the context in which it is
// embedded, or by the defaults for each feature in the case of the top-level
// document.
//
// Container Policy
// ----------------
// A declared policy can be set on a specific frame by the embedding page using
// the iframe "allow" attribute, or through attributes such as "allowfullscreen"
// or "allowpaymentrequest". This is the container policy for the embedded
// frame.
//
// Defaults
// --------
// Each defined feature has a default policy, which determines whether the
// feature is available when no policy has been declared, ans determines how the
// feature is inherited across origin boundaries.
//
// If the default policy  is in effect for a frame, then it controls how the
// feature is inherited by any cross-origin iframes embedded by the frame. (See
// the comments below in FeaturePolicy::DefaultPolicy for specifics)
//
// Policy Inheritance
// ------------------
// Policies in effect for a frame are inherited by any child frames it embeds.
// Unless another policy is declared in the child, all same-origin children will
// receive the same set of enables features as the parent frame. Whether or not
// features are inherited by cross-origin iframes without an explicit policy is
// determined by the feature's default policy. (Again, see the comments in
// FeaturePolicy::DefaultPolicy for details)

// This struct holds feature policy whitelist data that needs to be replicated
// between a RenderFrame and any of its associated RenderFrameProxies. A list of
// these form a ParsedFeaturePolicyHeader.
// NOTE: These types are used for replication frame state between processes.
// They exist only because we can't transfer WebVectors directly over IPC.
struct CONTENT_EXPORT ParsedFeaturePolicyDeclaration {
  ParsedFeaturePolicyDeclaration();
  ParsedFeaturePolicyDeclaration(std::string feature_name,
                                 bool matches_all_origins,
                                 std::vector<url::Origin> origins);
  ParsedFeaturePolicyDeclaration(const ParsedFeaturePolicyDeclaration& rhs);
  ~ParsedFeaturePolicyDeclaration();

  std::string feature_name;
  bool matches_all_origins;
  std::vector<url::Origin> origins;
};

using ParsedFeaturePolicyHeader = std::vector<ParsedFeaturePolicyDeclaration>;

class CONTENT_EXPORT FeaturePolicy {
 public:
  // Represents a collection of origins which make up a whitelist in a feature
  // policy. This collection may be set to match every origin (corresponding to
  // the "*" syntax in the policy string, in which case the Contains() method
  // will always return true.
  class Whitelist final {
   public:
    Whitelist();
    ~Whitelist();

    // Adds a single origin to the whitelist.
    void Add(const url::Origin& origin);

    // Adds all origins to the whitelist.
    void AddAll();

    // Returns true if the given origin has been added to the whitelist.
    bool Contains(const url::Origin& origin) const;

   private:
    bool matches_all_origins_;
    std::vector<url::Origin> origins_;
  };

  // The FeaturePolicy::FeatureDefault enum defines the default enable state for
  // a feature when neither it nor any parent frame have declared an explicit
  // policy. The three possibilities map directly to Feature Policy Whitelist
  // semantics.
  enum class FeatureDefault {
    // Equivalent to []. If this default policy is in effect for a frame, then
    // the feature will not be enabled for that frame or any of its children.
    DisableForAll,

    // Equivalent to ["self"]. If this default policy is in effect for a frame,
    // then the feature will be enabled for that frame, and any same-origin
    // child frames, but not for any cross-origin child frames.
    EnableForSelf,

    // Equivalent to ["*"]. If in effect for a frame, then the feature is
    // enabled for that frame and all of its children.
    EnableForAll
  };

  // The FeaturePolicy::Feature struct is used to define all features under
  // control of Feature Policy. There should only be one instance of this struct
  // for any given feature (declared below).
  struct Feature {
    // The name of the feature, as it should appear in a policy string
    const char* const feature_name;

    // Controls whether the feature should be available in the platform by
    // default, in the absence of any declared policy.
    FeatureDefault default_policy;
  };

  using FeatureList =
      std::map<blink::WebFeaturePolicyFeature, const FeaturePolicy::Feature*>;

  ~FeaturePolicy();

  static std::unique_ptr<FeaturePolicy> CreateFromParentPolicy(
      const FeaturePolicy* parent_policy,
      const ParsedFeaturePolicyHeader* container_policy,
      const url::Origin& origin);

  // Returns whether or not the given feature is enabled by this policy.
  bool IsFeatureEnabledForOrigin(blink::WebFeaturePolicyFeature feature,
                                 const url::Origin& origin) const;

  // Returns whether or not the given feature is enabled for the origin of the
  // document that owns the policy.
  bool IsFeatureEnabled(blink::WebFeaturePolicyFeature feature) const;

  // Sets the declared policy from the parsed Feature-Policy HTTP header.
  // Unrecognized features will be ignored.
  void SetHeaderPolicy(const ParsedFeaturePolicyHeader& parsed_header);

 private:
  friend class FeaturePolicyTest;

  explicit FeaturePolicy(url::Origin origin);
  FeaturePolicy(url::Origin origin, const FeatureList& feature_list);
  static std::unique_ptr<FeaturePolicy> CreateFromParentPolicy(
      const FeaturePolicy* parent_policy,
      const ParsedFeaturePolicyHeader* container_policy,
      const url::Origin& origin,
      const FeatureList& features);

  // Updates the inherited policy with the declarations from the iframe allow*
  // attributes.
  void AddContainerPolicy(const ParsedFeaturePolicyHeader* container_policy,
                          const FeaturePolicy* parent_policy);

  // Returns the list of features which can be controlled by Feature Policy.
  static const FeatureList& GetDefaultFeatureList();

  url::Origin origin_;

  // Map of feature names to declared whitelists. Any feature which is missing
  // from this map should use the inherited policy.
  std::map<blink::WebFeaturePolicyFeature, std::unique_ptr<Whitelist>>
      whitelists_;

  // Records whether or not each feature was enabled for this frame by its
  // parent frame.
  // TODO(iclelland): Generate, instead of this map, a set of bool flags, one
  // for each feature, as all features are supposed to be represented here.
  std::map<blink::WebFeaturePolicyFeature, bool> inherited_policies_;

  const FeatureList& feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_
