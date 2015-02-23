// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_RULES_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_RULES_H_

#include <vector>

#include "base/memory/linked_ptr.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/coalesced_permission_message.h"

namespace extensions {

// A formatter which generates a single permission message for all permissions
// that match a given rule. All remaining permissions that match the given rule
// are bucketed together, then GetPermissionMessage() is called with the
// resultant permission set.
// Note: since a ChromePermissionMessageFormatter can only produce a single
// CoalescedPermissionMessage and applies to all permissions with a given ID,
// this maintains the property that one permission ID can still only produce one
// message (that is, multiple permissions with the same ID cannot appear in
// multiple messages).
class ChromePermissionMessageFormatter {
 public:
  ChromePermissionMessageFormatter() {}
  virtual ~ChromePermissionMessageFormatter() {}

  // Returns the permission message for the given set of |permissions|.
  // |permissions| is guaranteed to have the IDs specified by the
  // required/optional permissions for the rule. The set will never be empty.
  virtual CoalescedPermissionMessage GetPermissionMessage(
      PermissionIDSet permissions) const = 0;

 private:
  // DISALLOW_COPY_AND_ASSIGN(ChromePermissionMessageFormatter);
};

// A simple rule to generate a coalesced permission message that stores the
// corresponding
// message ID for a given set of permission IDs. This rule generates the message
// with the given |message_id| if all the |required| permissions are present. If
// any |optional| permissions are present, they are also 'absorbed' by the rule
// to generate the final coalesced message.
// An optional |formatter| can be provided, which decides how the final
// CoalescedPermissionMessage appears. The default formatter simply displays the
// message with the ID |message_id|.
// Once a permission is used in a rule, it cannot apply to any future rules.
// TODO(sashab): Move all ChromePermissionMessageFormatters to their own
// provider class and remove ownership from ChromePermissionMessageRule.
class ChromePermissionMessageRule {
 public:
  // Returns all the rules used to generate permission messages for Chrome apps
  // and extensions.
  static std::vector<ChromePermissionMessageRule> GetAllRules();

  // Convenience constructors to allow inline initialization of the permission
  // ID sets.
  class PermissionIDSetInitializer : public std::set<APIPermission::ID> {
   public:
    // Don't make the constructor explicit to make the usage convenient.
    PermissionIDSetInitializer();
    PermissionIDSetInitializer(
        APIPermission::ID permission_one);  // NOLINT(runtime/explicit)
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two);
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three);
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three,
                               APIPermission::ID permission_four);
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three,
                               APIPermission::ID permission_four,
                               APIPermission::ID permission_five);
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three,
                               APIPermission::ID permission_four,
                               APIPermission::ID permission_five,
                               APIPermission::ID permission_six);
    virtual ~PermissionIDSetInitializer();
  };

  // Create a rule using the default formatter (display the message with ID
  // |message_id|).
  ChromePermissionMessageRule(int message_id,
                              PermissionIDSetInitializer required,
                              PermissionIDSetInitializer optional);
  // Create a rule with a custom formatter. Takes ownership of |formatter|.
  ChromePermissionMessageRule(ChromePermissionMessageFormatter* formatter,
                              PermissionIDSetInitializer required,
                              PermissionIDSetInitializer optional);
  virtual ~ChromePermissionMessageRule();

  std::set<APIPermission::ID> required_permissions() const;
  std::set<APIPermission::ID> optional_permissions() const;
  std::set<APIPermission::ID> all_permissions() const;
  ChromePermissionMessageFormatter* formatter() const;

 private:
  std::set<APIPermission::ID> required_permissions_;
  std::set<APIPermission::ID> optional_permissions_;

  // Owned by this class. linked_ptr is needed because this object is copyable
  // and stored in a returned vector.
  linked_ptr<ChromePermissionMessageFormatter> formatter_;

  // DISALLOW_COPY_AND_ASSIGN(ChromePermissionMessageRule);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_RULES_H_
