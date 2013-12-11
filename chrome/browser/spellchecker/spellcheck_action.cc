// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_action.h"

#include "base/logging.h"
#include "base/values.h"

SpellcheckAction::SpellcheckAction() : type(TYPE_PENDING), index(-1) {}

SpellcheckAction::SpellcheckAction(SpellcheckActionType type,
                                   int index,
                                   base::string16 value)
    : type(type), index(index), value(value) {}

SpellcheckAction::~SpellcheckAction() {}

bool SpellcheckAction::IsFinal() const {
  return type == TYPE_ADD_TO_DICT ||
         type == TYPE_IGNORE ||
         type == TYPE_IN_DICTIONARY ||
         type == TYPE_MANUALLY_CORRECTED ||
         type == TYPE_NO_ACTION ||
         type == TYPE_SELECT;
}

void SpellcheckAction::Finalize() {
  switch (type) {
    case TYPE_PENDING:
      type = TYPE_NO_ACTION;
      break;
    case TYPE_PENDING_IGNORE:
      type = TYPE_IGNORE;
      break;
    default:
      DCHECK(IsFinal());
      break;
  }
}

base::DictionaryValue* SpellcheckAction::Serialize() const {
  base::DictionaryValue* result = new base::DictionaryValue;
  switch (type) {
    case TYPE_SELECT:
      result->SetString("actionType", "SELECT");
      result->SetInteger("actionTargetIndex", index);
      break;
    case TYPE_ADD_TO_DICT:
      result->SetString("actionType", "ADD_TO_DICT");
      break;
    case TYPE_IGNORE:
      result->SetString("actionType", "IGNORE");
      break;
    case TYPE_IN_DICTIONARY:
      result->SetString("actionType", "IN_DICTIONARY");
      break;
    case TYPE_NO_ACTION:
      result->SetString("actionType", "NO_ACTION");
      break;
    case TYPE_MANUALLY_CORRECTED:
      result->SetString("actionType", "MANUALLY_CORRECTED");
      result->SetString("actionTargetValue", value);
      break;
    case TYPE_PENDING:
    case TYPE_PENDING_IGNORE:
      result->SetString("actionType", "PENDING");
      break;
    default:
      NOTREACHED();
      break;
  }
  return result;
}
