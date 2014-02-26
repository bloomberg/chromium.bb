// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_address_validator.h"

namespace autofill {

MockAddressValidator::MockAddressValidator() {
  using testing::_;
  using testing::Return;
  ON_CALL(*this, ValidateAddress(_, _, _)).WillByDefault(Return(SUCCESS));
  ON_CALL(*this, GetSuggestions(_, _, _, _)).WillByDefault(Return(SUCCESS));
  ON_CALL(*this, CanonicalizeAdministrativeArea(_)).WillByDefault(Return(true));
}

MockAddressValidator::~MockAddressValidator() {}

}  // namespace autofill
