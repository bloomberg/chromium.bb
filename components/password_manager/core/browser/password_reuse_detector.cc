// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include <algorithm>
#include <utility>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

using url::Origin;

namespace password_manager {

namespace {
// Minimum number of characters in a password for finding it as password reuse.
// It does not make sense to consider short strings for password reuse, since it
// is quite likely that they are parts of common words.
constexpr size_t kMinPasswordLengthToCheck = 8;

// Returns true iff |suffix_candidate| is a suffix of |str|.
bool IsSuffix(const base::string16& str,
              const base::string16& suffix_candidate) {
  if (str.size() < suffix_candidate.size())
    return false;
  return std::equal(suffix_candidate.rbegin(), suffix_candidate.rend(),
                    str.rbegin());
}

}  // namespace

const char kSyncPasswordDomain[] = "CHROME SYNC";

bool ReverseStringLess::operator()(const base::string16& lhs,
                                   const base::string16& rhs) const {
  return std::lexicographical_compare(lhs.rbegin(), lhs.rend(), rhs.rbegin(),
                                      rhs.rend());
}

PasswordReuseDetector::PasswordReuseDetector() {}

PasswordReuseDetector::~PasswordReuseDetector() {}

void PasswordReuseDetector::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  for (const auto& form : results)
    AddPassword(*form);
}

void PasswordReuseDetector::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  for (const auto& change : changes) {
    if (change.type() == PasswordStoreChange::ADD ||
        change.type() == PasswordStoreChange::UPDATE)
      AddPassword(change.form());
  }
}

void PasswordReuseDetector::CheckReuse(
    const base::string16& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK(consumer);
  if (input.size() < kMinPasswordLengthToCheck)
    return;

  if (CheckSyncPasswordReuse(input, domain, consumer))
    return;

  if (CheckSavedPasswordReuse(input, domain, consumer))
    return;
}

bool PasswordReuseDetector::CheckSyncPasswordReuse(
    const base::string16& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  if (!sync_password_data_.has_value())
    return false;

  const Origin gaia_origin(GaiaUrls::GetInstance()->gaia_url().GetOrigin());
  if (Origin(GURL(domain)).IsSameOriginWith(gaia_origin))
    return false;

  // TODO(crbug.com/657041): Implement checking a hash of |input| with
  // |sync_password_data_|.

  return false;
}

bool PasswordReuseDetector::CheckSavedPasswordReuse(
    const base::string16& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  const std::string registry_controlled_domain =
      GetRegistryControlledDomain(GURL(domain));
  auto passwords_iterator = FindSavedPassword(input);
  if (passwords_iterator == passwords_.end())
    return false;

  const std::set<std::string>& domains = passwords_iterator->second;
  DCHECK(!domains.empty());
  if (domains.find(registry_controlled_domain) == domains.end()) {
    // Return only one domain.
    const std::string& legitimate_domain = *domains.begin();
    consumer->OnReuseFound(passwords_iterator->first, legitimate_domain,
                           saved_passwords_, domains.size());
    return true;
  }

  return false;
}

void PasswordReuseDetector::UseSyncPasswordHash(
    base::Optional<SyncPasswordData> sync_password_data) {
  sync_password_data_ = std::move(sync_password_data);
}

void PasswordReuseDetector::ClearSyncPasswordHash() {
  sync_password_data_.reset();
}

void PasswordReuseDetector::AddPassword(const autofill::PasswordForm& form) {
  if (form.password_value.size() < kMinPasswordLengthToCheck)
    return;
  GURL signon_realm(form.signon_realm);
  const std::string domain = GetRegistryControlledDomain(signon_realm);
  std::set<std::string>& domains = passwords_[form.password_value];
  if (domains.find(domain) == domains.end()) {
    ++saved_passwords_;
    domains.insert(domain);
  }
}

PasswordReuseDetector::passwords_iterator
PasswordReuseDetector::FindSavedPassword(const base::string16& input) {
  // Keys in |passwords_| are ordered by lexicographical order of reversed
  // strings. In order to check a password reuse a key of |passwords_| that is a
  // suffix of |input| should be found. The longest such key should be the
  // largest key in the |passwords_| keys order that is equal or smaller to
  // |input|.
  if (passwords_.empty())
    return passwords_.end();

  // lower_bound returns the first key that is bigger or equal to input.
  auto it = passwords_.lower_bound(input);
  if (it != passwords_.end() && it->first == input) {
    // If the key is equal then a saved password is found.
    return it;
  }

  // Otherwise the previous key is a candidate for password reuse.
  if (it == passwords_.begin())
    return passwords_.end();

  --it;
  return IsSuffix(input, it->first) ? it : passwords_.end();
}

}  // namespace password_manager
