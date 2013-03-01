// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/page_action_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler_helpers.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace extensions {

PageActionHandler::PageActionHandler() {
}

PageActionHandler::~PageActionHandler() {
}

bool PageActionHandler::Parse(Extension* extension, string16* error) {
  scoped_ptr<ActionInfo> page_action_info;
  const DictionaryValue* page_action_value = NULL;

  if (extension->manifest()->HasKey(keys::kPageActions)) {
    const ListValue* list_value = NULL;
    if (!extension->manifest()->GetList(keys::kPageActions, &list_value)) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionsList);
      return false;
    }

    size_t list_value_length = list_value->GetSize();

    if (list_value_length == 0u) {
      // A list with zero items is allowed, and is equivalent to not having
      // a page_actions key in the manifest.  Don't set |page_action_value|.
    } else if (list_value_length == 1u) {
      if (!list_value->GetDictionary(0, &page_action_value)) {
        *error = ASCIIToUTF16(errors::kInvalidPageAction);
        return false;
      }
    } else {  // list_value_length > 1u.
      *error = ASCIIToUTF16(errors::kInvalidPageActionsListSize);
      return false;
    }
  } else if (extension->manifest()->HasKey(keys::kPageAction)) {
    if (!extension->manifest()->GetDictionary(keys::kPageAction,
                                              &page_action_value)) {
      *error = ASCIIToUTF16(errors::kInvalidPageAction);
      return false;
    }
  }

  // If page_action_value is not NULL, then there was a valid page action.
  if (page_action_value) {
    page_action_info = manifest_handler_helpers::LoadActionInfo(
        extension, page_action_value, error);
    if (!page_action_info)
      return false;  // Failed to parse page action definition.
  }
  ActionInfo::SetPageActionInfo(extension, page_action_info.release());

  return true;
}

const std::vector<std::string> PageActionHandler::Keys() const {
  std::vector<std::string> keys;
  keys.push_back(keys::kPageAction);
  keys.push_back(keys::kPageActions);
  return keys;
}

}  // namespace extensions
