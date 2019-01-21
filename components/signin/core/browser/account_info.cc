// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_info.h"

namespace {

// Updates |field| with |new_value| if non-empty and different; if |new_value|
// is equal to |default_value| then it won't override |field| unless it is not
// set. Returns whether |field| was changed.
bool UpdateField(std::string* field,
                 const std::string& new_value,
                 const char* default_value) {
  if (*field == new_value || new_value.empty())
    return false;

  if (!field->empty() && default_value && new_value == default_value)
    return false;

  *field = new_value;
  return true;
}

// Updates |field| with |new_value| if true. Returns whether |field| was
// changed.
bool UpdateField(bool* field, bool new_value) {
  if (*field == new_value || !new_value)
    return false;

  *field = new_value;
  return true;
}

}  // namespace

// This must be a string which can never be a valid domain.
const char kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";

// This must be a string which can never be a valid picture URL.
const char kNoPictureURLFound[] = "NO_PICTURE_URL";

AccountInfo::AccountInfo() = default;

AccountInfo::~AccountInfo() = default;

AccountInfo::AccountInfo(const AccountInfo& other) = default;

AccountInfo::AccountInfo(AccountInfo&& other) noexcept = default;

AccountInfo& AccountInfo::operator=(const AccountInfo& other) = default;

AccountInfo& AccountInfo::operator=(AccountInfo&& other) noexcept = default;

bool AccountInfo::IsEmpty() const {
  return account_id.empty() && email.empty() && gaia.empty() &&
         hosted_domain.empty() && full_name.empty() && given_name.empty() &&
         locale.empty() && picture_url.empty();
}

bool AccountInfo::IsValid() const {
  return !account_id.empty() && !email.empty() && !gaia.empty() &&
         !hosted_domain.empty() && !full_name.empty() && !given_name.empty() &&
         !locale.empty() && !picture_url.empty();
}

bool AccountInfo::UpdateWith(const AccountInfo& other) {
  if (account_id != other.account_id) {
    // Only updates with a compatible AccountInfo.
    return false;
  }

  bool modified = false;
  modified |= UpdateField(&gaia, other.gaia, nullptr);
  modified |= UpdateField(&email, other.email, nullptr);
  modified |= UpdateField(&full_name, other.full_name, nullptr);
  modified |= UpdateField(&given_name, other.given_name, nullptr);
  modified |=
      UpdateField(&hosted_domain, other.hosted_domain, kNoHostedDomainFound);
  modified |= UpdateField(&locale, other.locale, nullptr);
  modified |= UpdateField(&picture_url, other.picture_url, kNoPictureURLFound);
  modified |= UpdateField(&is_child_account, other.is_child_account);
  modified |= UpdateField(&is_under_advanced_protection,
                          other.is_under_advanced_protection);

  return modified;
}

AccountId AccountIdFromAccountInfo(const AccountInfo& account_info) {
  if (account_info.IsEmpty())
    return EmptyAccountId();

  DCHECK(!account_info.email.empty() && !account_info.gaia.empty());
  return AccountId::FromUserEmailGaiaId(account_info.email, account_info.gaia);
}
