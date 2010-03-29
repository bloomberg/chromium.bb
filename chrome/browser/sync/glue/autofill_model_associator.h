// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/abstract_autofill_model_associator.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/autofill_entry.h"

class ProfileSyncService;
class WebDatabase;

namespace browser_sync {

class AutofillChangeProcessor;
class UnrecoverableErrorHandler;

class AutofillModelAssociator : public AssociatorInterface {
 public:
  class ForFormfill : public AbstractAutofillModelAssociator<AutofillKey> {
   public:
    ForFormfill(ProfileSyncService* sync_service,
                WebDatabase* web_database,
                UnrecoverableErrorHandler* error_handler);
    // AbstractAutofillModelAssociator implementation.
    virtual bool AssociateModels();

    static std::string KeyToTag(const string16& name, const string16& value);
    static bool MergeTimestamps(const sync_pb::AutofillSpecifics& autofill,
                                const std::vector<base::Time>& timestamps,
                                std::vector<base::Time>* new_timestamps);
  };

  class ForProfiles : public AbstractAutofillModelAssociator<std::string> {
   public:
    ForProfiles(ProfileSyncService* sync_service,
                WebDatabase* web_database,
                UnrecoverableErrorHandler* error_handler);
    // AbstractAutofillModelAssociator implementation.
    virtual bool AssociateModels();
  };

  AutofillModelAssociator(ProfileSyncService* sync_service,
                          WebDatabase* web_database,
                          UnrecoverableErrorHandler* error_handler);

  // AssociatorInterface implementation.
  virtual bool AssociateModels();
  virtual bool DisassociateModels();
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool ChromeModelHasUserCreatedNodes(bool* has_nodes);

  ForFormfill* for_formfill() { return &for_formfill_; }
  ForProfiles* for_profiles() { return &for_profiles_; }

 private:
  ForFormfill for_formfill_;
  ForProfiles for_profiles_;
  DISALLOW_COPY_AND_ASSIGN(AutofillModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
