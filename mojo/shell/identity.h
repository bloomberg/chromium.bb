// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_IDENTITY_H_
#define MOJO_SHELL_IDENTITY_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>

namespace mojo {
namespace shell {

// A set of names of interfaces that may be exposed to an application.
using AllowedInterfaces = std::set<std::string>;
// A map of allowed applications to allowed interface sets. See shell.mojom for
// more details.
using CapabilityFilter = std::map<std::string, AllowedInterfaces>;


// Represents the identity of an application.
// |name| is the structured name of the application.
// |qualifier| is a string that allows to tie a specific instance of an
// application to another. A typical use case of qualifier is to control process
// grouping for a given application name. For example, the core services are
// grouped into "Core"/"Files"/"Network"/etc. using qualifier; content handler's
// qualifier is derived from the origin of the content.
class Identity {
 public:
  Identity();
  // Assumes user = mojom::Connector::kUserRoot.
  // Used in tests or for shell-initiated connections.
  explicit Identity(const std::string& in_name);
  Identity(const std::string& in_name,
           const std::string& in_qualifier,
           uint32_t user_id);
  Identity(const Identity& other);
  ~Identity();

  bool operator<(const Identity& other) const;
  bool is_null() const { return name_.empty(); }
  bool operator==(const Identity& other) const;

  const std::string& name() const { return name_; }
  uint32_t user_id() const { return user_id_; }
  void set_user_id(uint32_t user_id) { user_id_ = user_id; }
  const std::string& qualifier() const { return qualifier_; }
  void set_filter(const CapabilityFilter& filter) { filter_ = filter; }
  const CapabilityFilter& filter() const { return filter_; }

 private:
  std::string name_;
  std::string qualifier_;

  uint32_t user_id_;

  // TODO(beng): CapabilityFilter is not currently included in equivalence
  //             checks for Identity since we're not currently clear on the
  //             policy for instance disambiguation. Need to figure this out.
  //             This field is supplied because it is logically part of the
  //             instance identity of an application.
  CapabilityFilter filter_;
};

// Creates an identity for the Shell, used when the Shell connects to
// applications.
Identity CreateShellIdentity();

// Returns a capability filter that allows an application to connect to any
// other application and any service exposed by other applications.
CapabilityFilter GetPermissiveCapabilityFilter();

// Returns the set of interfaces that an application instance with |filter| is
// allowed to see from an instance with |identity|.
AllowedInterfaces GetAllowedInterfaces(const CapabilityFilter& filter,
                                       const Identity& identity);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_IDENTITY_H_
