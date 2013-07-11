// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_MOCK_WALLET_CLIENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_MOCK_WALLET_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
namespace wallet {

// A mock version of WalletClient that never issues real requests, just records
// mock calls to each entry point.
class MockWalletClient : public WalletClient {
 public:
  MockWalletClient(net::URLRequestContextGetter* context,
                   WalletClientDelegate* delegate);
  virtual ~MockWalletClient();

  MOCK_METHOD1(GetWalletItems, void(const GURL& source_url));

  MOCK_METHOD3(AcceptLegalDocuments,
      void(const std::vector<WalletItems::LegalDocument*>& documents,
           const std::string& google_transaction_id,
           const GURL& source_url));

  MOCK_METHOD3(AuthenticateInstrument,
      void(const std::string& instrument_id,
           const std::string& card_verification_number,
           const std::string& obfuscated_gaia_id));

  MOCK_METHOD1(GetFullWallet,
      void(const WalletClient::FullWalletRequest& request));

  MOCK_METHOD2(SaveAddress,
      void(const Address& address, const GURL& source_url));

  MOCK_METHOD3(SaveInstrument,
      void(const Instrument& instrument,
           const std::string& obfuscated_gaia_id,
           const GURL& source_url));

  MOCK_METHOD4(SaveInstrumentAndAddress,
      void(const Instrument& instrument,
           const Address& address,
           const std::string& obfuscated_gaia_id,
           const GURL& source_url));

  MOCK_METHOD4(SendAutocheckoutStatus,
      void(autofill::AutocheckoutStatus status,
           const GURL& source_url,
           const std::vector<AutocheckoutStatistic>& latency_statistics,
           const std::string& google_transaction_id));

  MOCK_METHOD2(UpdateAddress,
      void(const Address& address, const GURL& source_url));

  virtual void UpdateInstrument(
      const WalletClient::UpdateInstrumentRequest& update_request,
      scoped_ptr<Address> billing_address) OVERRIDE {
    updated_billing_address_ = billing_address.Pass();
  }

  const Address* updated_billing_address() {
    return updated_billing_address_.get();
  }

 private:
  scoped_ptr<Address> updated_billing_address_;

  DISALLOW_COPY_AND_ASSIGN(MockWalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_MOCK_WALLET_CLIENT_H_
