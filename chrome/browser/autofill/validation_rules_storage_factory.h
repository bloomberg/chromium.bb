// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_VALIDATION_RULES_STORAGE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_VALIDATION_RULES_STORAGE_FACTORY_H_

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace i18n {
namespace addressinput {
class Storage;
}
}

class JsonPrefStore;
class PersistentPrefStore;

namespace autofill {

// Creates Storage objects, all of which are backed by a common pref store.
class ValidationRulesStorageFactory {
 public:
  static scoped_ptr< ::i18n::addressinput::Storage> CreateStorage();

 private:
  friend struct base::DefaultLazyInstanceTraits<ValidationRulesStorageFactory>;

  ValidationRulesStorageFactory();
  ~ValidationRulesStorageFactory();

  scoped_refptr<JsonPrefStore> json_pref_store_;

  DISALLOW_COPY_AND_ASSIGN(ValidationRulesStorageFactory);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_VALIDATION_RULES_STORAGE_FACTORY_H_
