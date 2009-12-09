// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"

class AutoFillManager;
class FormStructure;

// Handles loading and saving AutoFill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// AutoFill.
class PersonalDataManager {
 public:
  virtual ~PersonalDataManager();

  // If AutoFill is able to determine the field types of a significant number
  // of field types that contain information in the FormStructures and the user
  // has not previously been prompted, the user will be asked if he would like
  // to import the data. If the user selects yes, a profile will be created
  // with all of the information from recognized fields.
  bool ImportFormData(const std::vector<FormStructure*>& form_structures,
                      AutoFillManager* autofill_manager);

  // Gets the possible field types for the given text, determined by matching
  // the text with all known personal information and returning matching types.
  void GetPossibleFieldTypes(const string16& text,
                             FieldTypeSet* possible_types);

 private:
  // Make sure that only Profile can create an instance of PersonalDataManager.
  friend class ProfileImpl;

  PersonalDataManager();
};

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
