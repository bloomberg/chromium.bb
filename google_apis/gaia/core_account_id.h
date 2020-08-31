// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
#define GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_

#include <ostream>
#include <string>
#include <vector>

#include "build/build_config.h"

// Represent the id of an account for interaction with GAIA.
//
// --------------------------------------------------------------------------
// DO NOT USE CoreAccountId AS A PERSISTENT IDENTIFIER OF AN ACCOUNT.
//
// Currently a CoreAccountId can be created from a Gaia ID or from an email
// that was canonicalized. We are in the process of migrating this identifier
// to always be created from a Gaia ID.
// Until the migration is complete, the value of a CoreAccountId value may
// change on start-up.
// --------------------------------------------------------------------------

struct CoreAccountId {
  CoreAccountId();
  CoreAccountId(const CoreAccountId&);
  CoreAccountId(CoreAccountId&&) noexcept;
  ~CoreAccountId();

  CoreAccountId& operator=(const CoreAccountId&);
  CoreAccountId& operator=(CoreAccountId&&) noexcept;

  // Checks if the account is valid or not.
  bool empty() const;

  // Returns true if this CoreAccountId was created from an email.
  // Returns false if it is empty.
  bool IsEmail() const;

  // Return the string representation of a CoreAccountID.
  //
  // As explained above, the string representation of a CoreAccountId is
  // (for now) unstable and cannot be used to store serialized data to
  // persistent storage. Only in-memory storage is safe.
  const std::string& ToString() const;

  // -------------------------------------------------------------------------
  // --------------------------- DO NOT USE ----------------------------------
  // TL;DR: To get a CoreAccountId, please use the IdentityManager.
  //
  // All constructors of this class are private or only used for tests as
  // clients should not be creating CoreAccountId objects directly.

  // Create a CoreAccountId from a Gaia ID.
  // Returns an empty CoreAccountId if |gaia_id| is empty.
  static CoreAccountId FromGaiaId(const std::string& gaia_id);

  // Create a CoreAccountId object from an email.
  // Returns an empty CoreAccountId if |email| is empty.
  static CoreAccountId FromEmail(const std::string& email);

  // Create a CoreAccountId object from a string that is either a gaia_id
  // or an email.
  //
  // Note: This method only exits while the code is being migrated to
  // use Gaia ID as the value of a CoreAccountId.
  static CoreAccountId FromString(const std::string gaia_id_or_email);
  // ---------------------------------------- ---------------------------------

  // --------------------------------------------------------------------------
  // -------------------- ONLY FOR TESTING ------------------------------------
  // The following constructors are only used for testing. Their implementation
  // is defined in core_account_id_for_testing.cc and is not linked in the
  // production code. The reason for this is that they are currently being
  // removed, but they are extensively used by the testing code.
  //
  // TODO(crbug.com/1028578): Update the tests to use one of FromEmail(),
  // FromGaia() or FromString() methods above.
  explicit CoreAccountId(const char* id);
  explicit CoreAccountId(std::string&& id);
  explicit CoreAccountId(const std::string& id);
  // --------------------------------------------------------------------------

 private:
  std::string id_;
};

bool operator<(const CoreAccountId& lhs, const CoreAccountId& rhs);

bool operator==(const CoreAccountId& lhs, const CoreAccountId& rhs);

bool operator!=(const CoreAccountId& lhs, const CoreAccountId& rhs);

std::ostream& operator<<(std::ostream& out, const CoreAccountId& a);

// Returns the values of the account ids in a vector. Useful especially for
// logs.
std::vector<std::string> ToStringList(
    const std::vector<CoreAccountId>& account_ids);

namespace std {
template <>
struct hash<CoreAccountId> {
  size_t operator()(const CoreAccountId& account_id) const {
    return std::hash<std::string>()(account_id.ToString());
  }
};
}  // namespace std

#endif  // GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
