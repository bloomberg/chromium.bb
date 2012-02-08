// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/declarative_api.h"

#include "base/values.h"

namespace extensions {

bool AddRulesFunction::RunImpl() {
  // LOG(ERROR) << "AddRulesFunction called";

  ListValue* rules_list = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &rules_list));

  // TODO(battre): Generate unique IDs and priorities here.

  result_.reset(rules_list->DeepCopy());
  return true;
}

bool RemoveRulesFunction::RunImpl() {
  // LOG(ERROR) << "RemoveRulesFunction called";
  return true;
}

bool GetRulesFunction::RunImpl() {
  // LOG(ERROR) << "GetRulesFunction called";
  result_.reset(new ListValue());
  return true;
}

}  // namespace extensions
