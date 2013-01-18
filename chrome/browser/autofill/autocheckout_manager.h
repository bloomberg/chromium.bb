// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_MANAGER_H_

#include <string>

#include "base/memory/weak_ptr.h"

class AutofillManager;
class FormStructure;
class GURL;

struct FormData;

namespace content {
struct SSLStatus;
}

class AutocheckoutManager {
 public:
  explicit AutocheckoutManager(AutofillManager* autofill_manager);

  virtual void ShowAutocheckoutDialog(const GURL& frame_url,
                                      const content::SSLStatus& ssl_status);

  virtual ~AutocheckoutManager();

 private:

  // Callback called from AutofillDialogController on filling up the UI form.
  void ReturnAutocheckoutData(const FormStructure* result);

  AutofillManager* autofill_manager_;  // WEAK; owns us
  base::WeakPtrFactory<AutocheckoutManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_MANAGER_H_
