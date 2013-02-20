// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_ENCRYPTION_ESCROW_CLIENT_OBSERVER_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_ENCRYPTION_ESCROW_CLIENT_OBSERVER_H_

#include <string>

namespace autofill {
namespace wallet {

// EncryptionEscrowClientObserver is to be implemented by any classes making
// calls with EncryptionEscrowClient. The appropriate callback method will be
// called on EncryptionEscrowClientObserver with the response from the Online
// Wallet encryption and escrow backend.
class EncryptionEscrowClientObserver {
 public:
  // Called when an EncryptOneTimePad request finishes successfully.
  // |encrypted_one_time_pad| and |session_material| must be used when getting a
  // FullWallet.
  virtual void OnDidEncryptOneTimePad(const std::string& encrypted_one_time_pad,
                                      const std::string& session_material) = 0;

  // Called when an EscrowSensitiveInformation request finishes successfully.
  // |escrow_handle| must be used when saving a new instrument.
  virtual void OnDidEscrowSensitiveInformation(
      const std::string& escrow_handle) = 0;

  // Called when a request fails due to a network error or if the response was
  // invalid.
  virtual void OnNetworkError(int response_code) = 0;

  // Called when a request fails due to a malformed response.
  virtual void OnMalformedResponse() = 0;

 protected:
  virtual ~EncryptionEscrowClientObserver() {}
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_ENCRYPTION_ESCROW_CLIENT_OBSERVER_H_
