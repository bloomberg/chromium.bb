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

  MOCK_METHOD2(AuthenticateInstrument,
      void(const std::string& instrument_id,
           const std::string& card_verification_number));

  MOCK_METHOD1(GetFullWallet,
      void(const WalletClient::FullWalletRequest& request));

  // Methods with scoped_ptrs can't be mocked but by using the implementation
  // below the same effect can be achieved.
  virtual void SaveToWallet(scoped_ptr<wallet::Instrument> instrument,
                            scoped_ptr<wallet::Address> address,
                            const GURL& source_url) OVERRIDE {
      SaveToWalletMock(instrument.get(), address.get(), source_url);
  }

  MOCK_METHOD3(SaveToWalletMock,
      void(Instrument* instrument,
           Address* address,
           const GURL& source_url));

  MOCK_METHOD4(SendAutocheckoutStatus,
      void(autofill::AutocheckoutStatus status,
           const GURL& source_url,
           const std::vector<AutocheckoutStatistic>& latency_statistics,
           const std::string& google_transaction_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_MOCK_WALLET_CLIENT_H_
