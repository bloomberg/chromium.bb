// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/error_map.h"

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace {

// The maximum number of errors to be stored per extension.
const size_t kMaxErrorsPerExtension = 100;

base::LazyInstance<ErrorList> g_empty_error_list = LAZY_INSTANCE_INITIALIZER;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ErrorMap::ExtensionEntry
ErrorMap::ExtensionEntry::ExtensionEntry() {
}

ErrorMap::ExtensionEntry::~ExtensionEntry() {
  DeleteAllErrors();
}

void ErrorMap::ExtensionEntry::DeleteAllErrors() {
  STLDeleteContainerPointers(list_.begin(), list_.end());
  list_.clear();
}

void ErrorMap::ExtensionEntry::DeleteIncognitoErrors() {
  ErrorList::iterator iter = list_.begin();
  while (iter != list_.end()) {
    if ((*iter)->from_incognito()) {
      delete *iter;
      iter = list_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void ErrorMap::ExtensionEntry::DeleteErrorsOfType(ExtensionError::Type type) {
  ErrorList::iterator iter = list_.begin();
  while (iter != list_.end()) {
    if ((*iter)->type() == type) {
      delete *iter;
      iter = list_.erase(iter);
    } else {
      ++iter;
    }
  }
}

const ExtensionError* ErrorMap::ExtensionEntry::AddError(
    scoped_ptr<ExtensionError> error) {
  for (ErrorList::iterator iter = list_.begin(); iter != list_.end(); ++iter) {
    // If we find a duplicate error, remove the old error and add the new one,
    // incrementing the occurrence count of the error. We use the new error
    // for runtime errors, so we can link to the latest context, inspectable
    // view, etc.
    if (error->IsEqual(*iter)) {
      error->set_occurrences((*iter)->occurrences() + 1);
      delete *iter;
      list_.erase(iter);
      break;
    }
  }

  // If there are too many errors for an extension already, limit ourselves to
  // the most recent ones.
  if (list_.size() >= kMaxErrorsPerExtension) {
    delete list_.front();
    list_.pop_front();
  }

  list_.push_back(error.release());
  return list_.back();
}

////////////////////////////////////////////////////////////////////////////////
// ErrorMap
ErrorMap::ErrorMap() {
}

ErrorMap::~ErrorMap() {
  RemoveAllErrors();
}

const ErrorList& ErrorMap::GetErrorsForExtension(
    const std::string& extension_id) const {
  EntryMap::const_iterator iter = map_.find(extension_id);
  return iter != map_.end() ? *iter->second->list() : g_empty_error_list.Get();
}

const ExtensionError* ErrorMap::AddError(scoped_ptr<ExtensionError> error) {
  EntryMap::iterator iter = map_.find(error->extension_id());
  if (iter == map_.end()) {
    iter = map_.insert(std::pair<std::string, ExtensionEntry*>(
        error->extension_id(), new ExtensionEntry)).first;
  }
  return iter->second->AddError(error.Pass());
}

void ErrorMap::Remove(const std::string& extension_id) {
  EntryMap::iterator iter = map_.find(extension_id);
  if (iter == map_.end())
    return;

  delete iter->second;
  map_.erase(iter);
}

void ErrorMap::RemoveErrorsForExtensionOfType(const std::string& extension_id,
                                              ExtensionError::Type type) {
  EntryMap::iterator iter = map_.find(extension_id);
  if (iter != map_.end())
    iter->second->DeleteErrorsOfType(type);
}

void ErrorMap::RemoveIncognitoErrors() {
  for (EntryMap::iterator iter = map_.begin(); iter != map_.end(); ++iter)
    iter->second->DeleteIncognitoErrors();
}

void ErrorMap::RemoveAllErrors() {
  for (EntryMap::iterator iter = map_.begin(); iter != map_.end(); ++iter) {
    iter->second->DeleteAllErrors();
    delete iter->second;
  }
  map_.clear();
}

}  // namespace extensions
