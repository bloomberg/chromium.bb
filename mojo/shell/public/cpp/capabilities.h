// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_CAPABILITIES_H_
#define MOJO_SHELL_PUBLIC_CPP_CAPABILITIES_H_

#include <map>
#include <set>
#include <string>

#include "mojo/shell/public/interfaces/shell_resolver.mojom.h"

namespace mojo {
using Class = std::string;
using Classes = std::set<std::string>;
using Interface = std::string;
using Interfaces = std::set<std::string>;
using Name = std::string;

// See comments in mojo/shell/public/interfaces/capabilities.mojom for a
// description of CapabilityRequest and CapabilitySpec.

struct CapabilityRequest {
  CapabilityRequest();
  CapabilityRequest(const CapabilityRequest& other);
  ~CapabilityRequest();
  bool operator==(const CapabilityRequest& other) const;
  bool operator<(const CapabilityRequest& other) const;
  Classes classes;
  Interfaces interfaces;
};

struct CapabilitySpec {
  CapabilitySpec();
  CapabilitySpec(const CapabilitySpec& other);
  ~CapabilitySpec();
  bool operator==(const CapabilitySpec& other) const;
  bool operator<(const CapabilitySpec& other) const;
  std::map<Class, Interfaces> provided;
  std::map<Name, CapabilityRequest> required;
};

template <>
struct TypeConverter<shell::mojom::CapabilitySpecPtr, CapabilitySpec> {
  static shell::mojom::CapabilitySpecPtr Convert(
      const CapabilitySpec& input);
};
template <>
struct TypeConverter<CapabilitySpec, shell::mojom::CapabilitySpecPtr> {
  static CapabilitySpec Convert(
      const shell::mojom::CapabilitySpecPtr& input);
};

template <>
struct TypeConverter<shell::mojom::CapabilityRequestPtr,
                     CapabilityRequest> {
  static shell::mojom::CapabilityRequestPtr Convert(
      const CapabilityRequest& input);
};
template <>
struct TypeConverter<CapabilityRequest,
                     shell::mojom::CapabilityRequestPtr> {
  static CapabilityRequest Convert(
      const shell::mojom::CapabilityRequestPtr& input);
};
}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_CAPABILITIES_H_
