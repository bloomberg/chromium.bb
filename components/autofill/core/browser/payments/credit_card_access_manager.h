// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

class AutofillManager;

// Manages logic for accessing credit cards either stored locally or stored
// with Google Payments. Owned by AutofillManager.
class CreditCardAccessManager : public CreditCardCVCAuthenticator::Requester {
 public:
  class Accessor {
   public:
    virtual ~Accessor() {}
    virtual void OnCreditCardFetched(
        bool did_succeed,
        const CreditCard* credit_card = nullptr,
        const base::string16& cvc = base::string16()) = 0;
  };

  explicit CreditCardAccessManager(AutofillManager* autofill_manager);
  CreditCardAccessManager(
      AutofillClient* client,
      PersonalDataManager* personal_data_manager,
      CreditCardFormEventLogger* credit_card_form_event_logger = nullptr);
  ~CreditCardAccessManager() override;

  // Logs information about current credit card data.
  void UpdateCreditCardFormEventLogger();
  // Returns all credit cards.
  std::vector<CreditCard*> GetCreditCards();
  // Returns credit cards in the order to be suggested to the user.
  std::vector<CreditCard*> GetCreditCardsToSuggest();
  // Returns true only if all cards are server cards.
  bool ShouldDisplayGPayLogo();
  // Returns true when deletion is allowed. Only local cards can be deleted.
  bool DeleteCard(const CreditCard* card);
  // Returns true if the |card| is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const CreditCard* card,
                                   base::string16* title,
                                   base::string16* body);

  // Returns false only if some form of authentication is still in progress.
  bool ShouldClearPreviewedForm();

  // Retrieves instance of CreditCard with given guid.
  CreditCard* GetCreditCard(std::string guid);

  // Calls |accessor->OnCreditCardFetched()| once credit card is fetched.
  void FetchCreditCard(const CreditCard* card,
                       base::WeakPtr<Accessor> accessor,
                       const base::TimeTicks& timestamp = base::TimeTicks());

  CreditCardCVCAuthenticator* credit_card_cvc_authenticator();

 private:
  friend class AutofillAssistantTest;
  friend class AutofillManagerTest;
  friend class AutofillMetricsTest;
  friend class CreditCardAccessManagerTest;

  // CreditCardCVCAuthenticator::Requester
  void OnCVCAuthenticationComplete(
      bool did_succeed,
      const CreditCard* card = nullptr,
      const base::string16& cvc = base::string16()) override;

  bool is_authentication_in_progress() {
    return is_authentication_in_progress_;
  }

  // Returns true only if |credit_card| is a local card.
  bool IsLocalCard(const CreditCard* credit_card);

  // Is set to true only when waiting for the callback to
  // OnCVCAuthenticationComplete() to be executed.
  bool is_authentication_in_progress_ = false;

  // The associated autofill client. Weak reference.
  AutofillClient* const client_;

  // Client to interact with Payments servers.
  payments::PaymentsClient* payments_client_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  // Weak reference.
  // May be NULL. NULL indicates OTR.
  PersonalDataManager* personal_data_manager_;

  // For logging metrics. May be NULL for tests.
  CreditCardFormEventLogger* form_event_logger_;

  // Authenticator for card requests.
  std::unique_ptr<CreditCardCVCAuthenticator> cvc_authenticator_;

  // The object attempting to access a card.
  base::WeakPtr<Accessor> accessor_;

  base::WeakPtrFactory<CreditCardAccessManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardAccessManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
